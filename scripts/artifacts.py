#!/usr/bin/env python3
"""Check each PlatformIO build and assemble a release with bounded flash writes."""

import argparse
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

BOARDS = {
    "esp32dev": ("ESP32", "uart", 4, 0x1000, 0x290000),
    "esp32-c3-devkitm-1": ("ESP32-C3", "uart", 4, 0, 0x290000),
    "esp32-wroom-32u": ("ESP32", "uart", 4, 0x1000, 0x290000),
    "esp12e": ("ESP8266", "uart", 4, 0, 0x300000),
    "esp32-s3-n16r8": ("ESP32-S3", "uart", 16, 0, 0xC90000),
}
RAM_BUDGET = {"esp12e": 43008}
SECTOR = 4096


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def measure(log, board):
    sizes = {}
    for region, used, capacity in re.findall(
        r"(RAM|Flash):\s*\[[^\]]*\].*?\(used (\d+) bytes from (\d+) bytes\)",
        log.read_text(encoding="utf-8"),
    ):
        used, capacity = int(used), int(capacity)
        limit = min(capacity * 9 // 10, RAM_BUDGET.get(board, capacity)) if region == "RAM" else capacity * 9 // 10
        if not 0 < used <= limit:
            raise ValueError(f"{board} {region}: {used} bytes exceeds budget of {limit}")
        sizes[region.lower()] = {"used": used, "limit": limit, "capacity": capacity}
    if sizes.keys() != {"ram", "flash"}:
        raise ValueError(f"{board}: missing PlatformIO RAM or flash measurements")
    return sizes


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stage(board, build, log, packages, output):
    family, console, flash_mb, boot_offset, config_offset = BOARDS[board]
    config_bytes = 0xFA000 if family == "ESP8266" else 0x10000
    sizes = measure(log, board)
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
        application = part("firmware", build / "firmware.bin", 0, sizes["flash"]["capacity"])
        install = [application]
        layout = "esp8266-4m1m-littlefs-v1"
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
            "serialType": console,
            "flashBytes": flash_mb * 1024 * 1024,
            "layout": layout,
            "storage": {"offset": config_offset, "size": config_bytes},
            "install": install,
            "update": [application],
            "resources": sizes,
        },
    )


def assemble(staged, output, version):
    output.mkdir(parents=True, exist_ok=True)
    profiles = []
    published = []
    for board in BOARDS:
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
                "builds": [{
                    "chipFamily": profile["chipFamily"],
                    "serialType": profile["serialType"],
                    "parts": [{"path": part["path"], "offset": part["offset"]} for part in profile["install"]],
                }],
            },
        )
        published.append(web_manifest)
        profile["espWebToolsInstall"] = web_manifest.name
        profiles.append(profile)

    manifest = output / "manifest.json"
    write_json(
        manifest,
        {"schemaVersion": 1, "name": "FirmwareEmbed", "version": version, "eraseAll": False, "profiles": profiles},
    )
    published.append(manifest)
    (output / "SHA256SUMS").write_text(
        "".join(f"{digest(path)}  {path.name}\n" for path in sorted(published)), encoding="ascii"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("stage")
    build.add_argument("board", choices=BOARDS)
    for name in ("build", "log", "packages", "output"):
        build.add_argument(f"--{name}", type=Path, required=True)
    release = sub.add_parser("assemble")
    release.add_argument("--staged", type=Path, required=True)
    release.add_argument("--output", type=Path, required=True)
    release.add_argument("--version", required=True)
    args = parser.parse_args()
    if args.command == "stage":
        stage(args.board, args.build, args.log, args.packages, args.output)
    else:
        assemble(args.staged, args.output, args.version)


if __name__ == "__main__":
    main()
