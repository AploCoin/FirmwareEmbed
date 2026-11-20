#!/usr/bin/env python3
"""Check each PlatformIO build and assemble a release with bounded flash writes."""

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


@dataclass(frozen=True)
class Project:
    id: str
    path: Path
    chip: str
    consoles: tuple[str, ...]
    flash_mb: int
    boot_offset: int
    config_offset: int
    config_bytes: int
    layout: str | None
    ram_budget: int | None


def read_project(path):
    config = configparser.ConfigParser(interpolation=None)
    with (path / "platformio.ini").open(encoding="utf-8") as source:
        config.read_file(source)
    board = config["platformio"]["default_envs"]
    if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_-]*", board):
        raise ValueError(f"{path}: expected one default environment")
    env = config[f"env:{board}"]
    libraries = re.findall(
        r"^https://github\.com/AploCoin/(AploEmbed|MiningEmbed)\.git$",
        env["lib_deps"], re.MULTILINE,
    )
    if set(libraries) != LIBRARIES or len(libraries) != len(LIBRARIES):
        raise ValueError(f"{path}: expected both AploCoin library URLs without refs")
    release = config["release"]
    consoles = tuple(value.strip() for value in release["serial_types"].split(","))
    if not set(consoles) <= {"cdc", "uart"} or len(set(consoles)) != len(consoles):
        raise ValueError(f"{path}: invalid serial_types")
    config_offset = int(release["storage_offset"], 0)
    config_bytes = int(release["storage_size"], 0)
    flash_mb = release.getint("flash_mib")
    if (
        config_offset <= 0 or config_bytes <= 0
        or config_offset % SECTOR or config_bytes % SECTOR
        or config_offset + config_bytes > flash_mb * 1024 * 1024
    ):
        raise ValueError(f"{path}: invalid storage range")
    return Project(
        board, path, release["chip"], consoles, flash_mb,
        int(release["bootloader_offset"], 0), config_offset, config_bytes,
        release.get("layout"), release.getint("static_ram_budget", fallback=None),
    )


def projects(directory):
    profiles = [read_project(path.parent) for path in sorted(directory.glob("*/platformio.ini"))]
    if not profiles or len({profile.id for profile in profiles}) != len(profiles):
        raise ValueError("expected at least one board and unique environment names")
    return profiles


def matrix(directory):
    profiles = projects(directory)
    return {"include": [
        {"id": profile.id, "directory": str(profile.path)} for profile in profiles
    ]}


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def measure(log, project):
    sizes = {}
    for region, used, capacity in re.findall(
        r"(RAM|Flash):\s*\[[^\]]*\].*?\(used (\d+) bytes from (\d+) bytes\)",
        log.read_text(encoding="utf-8"),
    ):
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


def stage(project, build, log, packages, output):
    board, family, consoles = project.id, project.chip, project.consoles
    flash_mb, boot_offset = project.flash_mb, project.boot_offset
    config_offset, config_bytes = project.config_offset, project.config_bytes
    sizes = measure(log, project)
    output.mkdir(parents=True, exist_ok=True)

    def part(kind, image, offset, limit):
        length = image.stat().st_size
        erase_end = offset + (length + SECTOR - 1) // SECTOR * SECTOR
        if offset % SECTOR or not length or erase_end > limit:
            raise ValueError(f"{board}/{kind} exceeds its flash range")
        asset = output / f"{board}-{kind}.bin"
        shutil.copyfile(image, asset)
        return {"path": asset.name, "offset": offset, "size": length, "sha256": digest(asset)}

    if family == "ESP8266":
        if boot_offset or not project.layout:
            raise ValueError(f"{board}: expected an ESP8266 layout ID and boot at zero")
        application = part("firmware", build / "firmware.bin", 0, sizes["flash"]["capacity"])
        install = [application]
        layout = project.layout
    else:
        table = build / "partitions.bin"
        entries = {}
        packed = table.read_bytes()
        for offset in range(0, len(packed), 32):
            magic, kind, subtype, start, length, name, _ = struct.unpack_from(
                "<HBBII16sI", packed, offset
            )
            if magic != 0x50AA:
                break
            entries[name.split(b"\0", 1)[0].decode("ascii")] = (kind, subtype, start, length)

        app0 = entries.get("app0")
        if not app0 or app0[:3] != (0, 0x10, 0x10000) or app0[3] != sizes["flash"]["capacity"]:
            raise ValueError(f"{board}: unexpected app0 size or offset")
        if entries.get("aplo_cfg") != (1, 2, config_offset, config_bytes):
            raise ValueError(f"{board}: unexpected dedicated NVS layout")
        if entries.get("otadata") != (1, 0, 0xE000, 0x2000):
            raise ValueError(f"{board}: unexpected otadata layout")

        application = part("firmware", build / "firmware.bin", app0[2], app0[2] + app0[3])
        boot_app0 = packages / "framework-arduinoespressif32/tools/partitions/boot_app0.bin"
        install = [
            part("bootloader", build / "bootloader.bin", boot_offset, 0x8000),
            part("partitions", table, 0x8000, 0x9000),
            part("boot-app0", boot_app0, 0xE000, 0x10000),
            application,
        ]
        layout = f"esp32-{flash_mb}mb-{digest(table)}"

    for image in install:
        end = image["offset"] + (image["size"] + SECTOR - 1) // SECTOR * SECTOR
        if end > flash_mb * 1024 * 1024 or (
            image["offset"] < config_offset + config_bytes and end > config_offset
        ):
            raise ValueError(f"{board}: asset overlaps configuration or exceeds physical flash")
    write_json(
        output / f"{board}.json",
        {
            "id": board,
            "chipFamily": family,
            "serialType": consoles[0],
            "serialTypes": list(consoles),
            "flashBytes": flash_mb * 1024 * 1024,
            "layout": layout,
            "storage": {"offset": config_offset, "size": config_bytes},
            "install": install,
            "update": [application],
            "resources": sizes,
        },
    )


def assemble(directory, staged, output, version, libraries=None):
    output.mkdir(parents=True, exist_ok=True)
    profiles = []
    published = []
    for project in projects(directory):
        board = project.id
        source = staged / board
        profile = json.loads((source / f"{board}.json").read_text(encoding="utf-8"))
        if profile["id"] != board:
            raise ValueError(f"{board}: stage belongs to another target")
        for part in profile["install"]:
            asset = source / part["path"]
            if asset.name != part["path"] or asset.stat().st_size != part["size"] or digest(asset) != part["sha256"]:
                raise ValueError(f"{board}: corrupt staged asset")
            destination = output / asset.name
            shutil.copyfile(asset, destination)
            published.append(destination)
        web_manifest = output / f"manifest-{board}.json"
        write_json(
            web_manifest,
            {
                "name": f"FirmwareEmbed ({board})",
                "version": version,
                "new_install_prompt_erase": True,
                "new_install_improv_wait_time": 0,
                "builds": [
                    {
                        "chipFamily": profile["chipFamily"],
                        "serialType": console,
                        "parts": [
                            {"path": part["path"], "offset": part["offset"]}
                            for part in profile["install"]
                        ],
                    }
                    for console in profile["serialTypes"]
                ],
            },
        )
        published.append(web_manifest)
        profile["espWebToolsInstall"] = web_manifest.name
        profiles.append(profile)

    manifest = output / "manifest.json"
    index = {
        "schemaVersion": 1, "name": "FirmwareEmbed", "version": version,
        "eraseAll": False, "profiles": profiles,
    }
    if libraries:
        index["libraries"] = libraries
    write_json(manifest, index)
    published.append(manifest)
    (output / "SHA256SUMS").write_text(
        "".join(f"{digest(path)}  {path.name}\n" for path in sorted(published)), encoding="ascii"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("stage")
    for name in ("project", "build", "log", "packages", "output"):
        build.add_argument(f"--{name}", type=Path, required=True)
    release = sub.add_parser("assemble")
    release.add_argument("--projects", type=Path, required=True)
    release.add_argument("--staged", type=Path, required=True)
    release.add_argument("--output", type=Path, required=True)
    release.add_argument("--version", required=True)
    release.add_argument("--sdk-sha")
    release.add_argument("--mining-sha")
    discover = sub.add_parser("matrix")
    discover.add_argument("--projects", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "stage":
        stage(read_project(args.project), args.build, args.log, args.packages, args.output)
    elif args.command == "assemble":
        libraries = None
        if args.sdk_sha or args.mining_sha:
            libraries = {"AploEmbed": args.sdk_sha, "MiningEmbed": args.mining_sha}
            if any(not re.fullmatch(r"[0-9a-f]{40}", sha or "") for sha in libraries.values()):
                release.error("expected both resolved 40-character library revisions")
        assemble(args.projects, args.staged, args.output, args.version, libraries)
    else:
        print(json.dumps(matrix(args.projects)))


if __name__ == "__main__":
    main()
