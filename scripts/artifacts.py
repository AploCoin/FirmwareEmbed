#!/usr/bin/env python3
"""Validate standalone builds and package sector-bounded USB-only firmware assets."""
import argparse
import configparser
from dataclasses import dataclass
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

SECTOR = 4096
LIBRARIES = {"AploEmbed", "MiningEmbed"}
SHA = re.compile(r"[0-9a-f]{40}")

@dataclass(frozen=True)
class Project:
    id: str
    path: Path
    chip: str
    consoles: tuple[str, ...]
    flash_bytes: int
    boot_offset: int
    config_offset: int
    config_bytes: int
    layout: str
    ram_budget: int | None
    supported: bool = True


def config_for(path):
    config = configparser.ConfigParser(interpolation=None)
    config.read(path / "platformio.ini", encoding="utf-8")
    return config


def read_project(path, environment=None):
    config = config_for(path)
    board = environment or config["platformio"]["default_envs"]
    if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_-]*", board):
        raise ValueError(f"{path}: expected one environment")
    env = dict(config["env"]) if "env" in config else {}
    env.update(config[f"env:{board}"])
    libraries = re.findall(r"^https://github\.com/AploCoin/(AploEmbed|MiningEmbed)\.git(?:#[0-9a-f]{40})?$", env["lib_deps"], re.MULTILINE)
    if set(libraries) != LIBRARIES or len(libraries) != 2:
        raise ValueError(f"{path}: expected both AploCoin library URLs")
    release = dict(config["release"])
    release.update(dict(config[f"release:{board}"]) if f"release:{board}" in config else {})
    consoles = tuple(value.strip() for value in release["serial_types"].split(","))
    if not consoles or not set(consoles) <= {"cdc", "uart"} or len(set(consoles)) != len(consoles):
        raise ValueError(f"{path}: invalid serial_types")
    start, length, flash = (int(release[key], 0) for key in ("storage_offset", "storage_size", "flash_bytes"))
    if start <= 0 or length <= 0 or start % SECTOR or length % SECTOR or start + length > flash:
        raise ValueError(f"{path}: invalid storage range")
    return Project(board, path, release["chip"], consoles, flash, int(release["bootloader_offset"], 0), start, length,
                   release["layout"], int(release["static_ram_budget"]) if "static_ram_budget" in release else None,
                   release.get("supported", "true").lower() == "true")


def projects(directory, include_unsupported=False):
    profiles = [read_project(path.parent, section[4:]) for path in sorted(directory.glob("*/platformio.ini"))
                for section in config_for(path.parent).sections() if section.startswith("env:")]
    if not profiles or len({p.id for p in profiles}) != len(profiles):
        raise ValueError("expected unique environment names")
    return [p for p in profiles if include_unsupported or p.supported]


def matrix(directory):
    return {"include": [{"id": p.id, "directory": str(p.path)} for p in projects(directory)]}


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def read_lock(path):
    lock = json.loads(path.read_text(encoding="utf-8"))
    if set(lock) != LIBRARIES or any(not isinstance(s, str) or not SHA.fullmatch(s) for s in lock.values()):
        raise ValueError("dependency lock must contain exactly two full immutable commit SHAs")
    return lock


def pin_project(path, lock):
    ini = path / "platformio.ini"
    text = ini.read_text(encoding="utf-8")
    for library, sha in lock.items():
        text, count = re.subn(rf"(https://github\.com/AploCoin/{library}\.git)(?:#[0-9a-f]{{40}})?(?=\s|$)", rf"\g<1>#{sha}", text)
        if count != 1:
            raise ValueError(f"expected exactly one {library} URL")
    ini.write_text(text, encoding="utf-8")


def measure(log, project):
    sizes = {}
    for region, used, capacity in re.findall(r"(RAM|Flash):\s*\[[^\]]*\].*?\(used (\d+) bytes from (\d+) bytes\)", log.read_text(encoding="utf-8")):
        used, capacity = int(used), int(capacity)
        limit = capacity * 9 // 10
        if region == "RAM" and project.ram_budget is not None:
            limit = min(limit, project.ram_budget)
        if not 0 < used <= limit:
            raise ValueError(f"{project.id} {region}: {used} bytes exceeds budget of {limit}")
        sizes[region.lower()] = {"used": used, "limit": limit, "capacity": capacity}
    if sizes.keys() != {"ram", "flash"}:
        raise ValueError(f"{project.id}: missing PlatformIO RAM or flash measurements")
    return sizes


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def partitions(path):
    entries = {}
    packed = path.read_bytes()
    if len(packed) % 32:
        raise ValueError("truncated partition table")
    for offset in range(0, len(packed), 32):
        magic, kind, subtype, start, length, name, _ = struct.unpack_from("<HBBII16sI", packed, offset)
        if magic != 0x50AA:
            break
        name = name.split(b"\0", 1)[0].decode("ascii")
        if name in entries:
            raise ValueError("duplicate partition")
        entries[name] = (kind, subtype, start, length)
    return entries


def stage(project, build, log, packages, output, version="dev"):
    if not project.supported:
        raise ValueError("unsupported diagnostic profiles cannot produce release artifacts")
    identity = json.loads((build / "identity.json").read_text(encoding="utf-8"))
    expected_identity = {"board": project.id, "version": version, "layout": project.layout,
                         "flashBytes": project.flash_bytes,
                         "storage": {"offset": project.config_offset, "size": project.config_bytes}}
    if identity != expected_identity:
        raise ValueError("build identity differs from staged board/version/layout")
    sizes = measure(log, project)
    output.mkdir(parents=True, exist_ok=True)
    def part(kind, image, offset, limit):
        length = image.stat().st_size
        end = offset + (length + SECTOR - 1) // SECTOR * SECTOR
        if offset % SECTOR or not length or end > limit:
            raise ValueError(f"{project.id}/{kind} exceeds its sector-bounded flash range")
        asset = output / f"{project.id}-{kind}.bin"
        shutil.copyfile(image, asset)
        return {"path": asset.name, "offset": offset, "size": length, "sha256": digest(asset)}
    if project.chip == "ESP8266":
        if project.boot_offset:
            raise ValueError("ESP8266 image must start at zero")
        application = part("firmware", build / "firmware.bin", 0, min(project.config_offset, sizes["flash"]["capacity"]))
        install = [application]
    else:
        table = build / "partitions.bin"
        entries = partitions(table)
        factory = entries.get("factory")
        if set(entries) != {"nvs", "factory", "aplo_cfg"} or not factory or factory != (0, 0, 0x10000, project.config_offset - 0x10000):
            raise ValueError(f"{project.id}: expected one factory application and no OTA partitions")
        if entries["nvs"] != (1, 2, 0x9000, 0x5000) or entries["aplo_cfg"] != (1, 2, project.config_offset, project.config_bytes):
            raise ValueError(f"{project.id}: unexpected NVS layout")
        if factory[3] != sizes["flash"]["capacity"]:
            raise ValueError("partition capacity differs from measured build capacity")
        application = part("firmware", build / "firmware.bin", factory[2], project.config_offset)
        install = [part("bootloader", build / "bootloader.bin", project.boot_offset, 0x8000),
                   part("partitions", table, 0x8000, 0x9000), application]
    for image in install:
        end = image["offset"] + (image["size"] + SECTOR - 1) // SECTOR * SECTOR
        if end > project.flash_bytes or image["offset"] < project.config_offset + project.config_bytes and end > project.config_offset:
            raise ValueError(f"{project.id}: write or erase overlaps configuration or physical flash")
    write_json(output / f"{project.id}.json", {
        "id": project.id, "version": version, "chipFamily": project.chip,
        "serialType": project.consoles[0], "serialTypes": list(project.consoles),
        "flashBytes": project.flash_bytes, "layout": project.layout,
        "storage": {"offset": project.config_offset, "size": project.config_bytes},
        "storageSchema": 2, "readableStorageSchemas": [1, 2], "ota": False,
        "install": install, "migrate": install, "update": [application], "resources": sizes,
    })


def assemble(directory, staged, output, version, libraries):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,63}", version):
        raise ValueError("invalid firmware version")
    if not libraries or set(libraries) != LIBRARIES or any(not SHA.fullmatch(v) for v in libraries.values()):
        raise ValueError("release requires both locked dependency SHAs")
    output.mkdir(parents=True, exist_ok=True)
    profiles, published = [], []
    for project in projects(directory):
        source = staged / project.id
        profile = json.loads((source / f"{project.id}.json").read_text(encoding="utf-8"))
        if profile["id"] != project.id or profile["version"] != version or profile["layout"] != project.layout:
            raise ValueError(f"{project.id}: staged identity/version/layout mismatch")
        if (profile["chipFamily"] != project.chip or profile["flashBytes"] != project.flash_bytes
                or profile["storage"] != {"offset": project.config_offset, "size": project.config_bytes}
                or profile.get("ota") is not False or profile["migrate"] != profile["install"]
                or len(profile["update"]) != 1 or profile["update"][0] not in profile["install"]
                or profile["update"][0]["offset"] != (0 if project.chip == "ESP8266" else 0x10000)):
            raise ValueError(f"{project.id}: staged profile violates USB update contract")
        for part in profile["install"]:
            asset = source / part["path"]
            if asset.name != part["path"] or asset.stat().st_size != part["size"] or digest(asset) != part["sha256"]:
                raise ValueError(f"{project.id}: corrupt staged asset")
            destination = output / asset.name
            shutil.copyfile(asset, destination)
            published.append(destination)
        profiles.append(profile)
    manifest = output / "manifest.json"
    write_json(manifest, {"schemaVersion": 2, "name": "FirmwareEmbed", "version": version,
                         "eraseAll": False, "ota": False, "profiles": profiles, "libraries": libraries})
    published.append(manifest)
    (output / "SHA256SUMS").write_text("".join(f"{digest(p)}  {p.name}\n" for p in sorted(published)), encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("stage")
    for name in ("project", "build", "log", "packages", "output"):
        build.add_argument(f"--{name}", type=Path, required=True)
    build.add_argument("--environment")
    build.add_argument("--version", default="dev")
    release = sub.add_parser("assemble")
    for name in ("projects", "staged", "output", "lock"):
        release.add_argument(f"--{name}", type=Path, required=True)
    release.add_argument("--version", required=True)
    discover = sub.add_parser("matrix")
    discover.add_argument("--projects", type=Path, required=True)
    pin = sub.add_parser("pin")
    pin.add_argument("--project", type=Path, required=True)
    pin.add_argument("--lock", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "stage":
        stage(read_project(args.project, args.environment), args.build, args.log, args.packages, args.output, args.version)
    elif args.command == "assemble":
        assemble(args.projects, args.staged, args.output, args.version, read_lock(args.lock))
    elif args.command == "pin":
        pin_project(args.project, read_lock(args.lock))
    else:
        print(json.dumps(matrix(args.projects)))

if __name__ == "__main__":
    main()
