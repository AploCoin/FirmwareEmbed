#!/usr/bin/env python3
"""Validate and plan a USB firmware write; execute only with --execute (esptool 4.x)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

SECTOR = 4096
CHIPS = {"ESP32": "esp32", "ESP32-C3": "esp32c3", "ESP32-S3": "esp32s3", "ESP8266": "esp8266"}
LEGACY = {"esp32dev": ("esp32-4m-dual-app-v1", 0x290000),
          "esp32-wroom-32u": ("esp32-4m-dual-app-v1", 0x290000),
          "esp32-c3-devkitm-1": ("esp32-4m-dual-app-v1", 0x290000),
          "esp32-s3-n16r8": ("esp32-s3-16m-dual-app-v1", 0xc90000)}


def device_info(text):
    text = text.strip()
    if text.startswith("{"):
        value = json.loads(text)
        if not isinstance(value, dict):
            raise ValueError("expected a GET_INFO object")
        return value
    if text.startswith("APLO1 OK "):
        text = text[len("APLO1 OK "):]
    if not text.startswith("info "):
        raise ValueError("expected GET_INFO JSON or info payload")
    return dict(field.split("=", 1) for field in text[5:].split())


def plan(manifest_path, board, mode, info=None, from_layout=None):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != 2 or manifest.get("eraseAll") is not False or manifest.get("ota") is not False:
        raise ValueError("expected USB-only manifest schema 2 with eraseAll=false")
    matches = [p for p in manifest["profiles"] if p["id"] == board]
    if len(matches) != 1:
        raise ValueError("select exactly one known board profile")
    profile = matches[0]
    if profile["version"] != manifest["version"] or profile.get("ota") is not False:
        raise ValueError("inconsistent profile identity")
    flash = profile["flashBytes"]
    start, length = profile["storage"]["offset"], profile["storage"]["size"]
    if any(type(n) is not int for n in (flash, start, length)) or start <= 0 or length <= 0 or start % SECTOR or length % SECTOR or start + length > flash:
        raise ValueError("invalid preserved configuration range")
    if mode == "update":
        if not info:
            raise ValueError("update requires a fresh GET_INFO capture from the connected device")
        expected = {"board": board, "layout": profile["layout"], "profile_flash_bytes": flash,
                    "storage_offset": start, "storage_size": length}
        for key, value in expected.items():
            if str(info.get(key)) != str(value):
                raise ValueError(f"device {key} does not match target profile; use a reviewed migration")
        if int(info.get("flash_bytes", 0)) < flash:
            raise ValueError("connected device has insufficient physical flash")
        if int(info.get("storage_schema", -1)) not in profile["readableStorageSchemas"]:
            raise ValueError("target cannot read current storage schema; rollback rejected")
    if mode == "migrate":
        legacy = LEGACY.get(board)
        if not legacy or from_layout != legacy[0] or start != legacy[1] or length != 0x10000:
            raise ValueError("only documented same-offset dual-app to factory migration is supported")
    parts = profile[mode]
    if not parts or mode == "update" and len(parts) != 1:
        raise ValueError("update must contain exactly one application image")
    application_offset = 0 if profile["chipFamily"] == "ESP8266" else 0x10000
    if mode == "update" and parts[0]["offset"] != application_offset:
        raise ValueError("update is not application-only")
    validated, ranges = [], []
    root = manifest_path.parent.resolve()
    for part in parts:
        path = root / part["path"]
        if Path(part["path"]).name != part["path"] or not path.is_file() or path.is_symlink():
            raise ValueError("asset must be a regular file beside the manifest")
        raw = path.read_bytes()
        offset, size = part["offset"], part["size"]
        if type(offset) is not int or type(size) is not int or offset < 0 or offset % SECTOR or not size or len(raw) != size or hashlib.sha256(raw).hexdigest() != part["sha256"]:
            raise ValueError("invalid asset size, offset or SHA-256")
        end = offset + (size + SECTOR - 1) // SECTOR * SECTOR
        if end > flash or offset < start + length and end > start or any(offset < b and end > a for a, b in ranges):
            raise ValueError("sector erase/write crosses preserved storage, another part or physical flash")
        ranges.append((offset, end))
        validated.append((offset, path))
    return profile, validated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--mode", choices=("install", "migrate", "update"), default="update")
    parser.add_argument("--device-info", type=Path, help="fresh configure.py info output saved to a file")
    parser.add_argument("--from-layout", help="documented legacy layout ID, required for migrate")
    parser.add_argument("--port", required=True)
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    try:
        info = device_info(args.device_info.read_text()) if args.device_info else None
        profile, parts = plan(args.manifest, args.profile, args.mode, info, args.from_layout)
        base = [sys.executable, "-m", "esptool", "--chip", CHIPS[profile["chipFamily"]], "--port", args.port]
        command = base + ["write_flash", "--flash_size", "keep"]
        for offset, path in parts:
            command += [hex(offset), str(path)]
        print(f"{args.mode}: {profile['id']} version={profile['version']} preserve={profile['storage']}")
        print(shlex.join(command))
        if args.execute:
            # Read physical size before any destructive command. esptool also verifies --chip.
            probe = subprocess.run(base + ["flash_id"], check=True, capture_output=True, text=True)
            detected = re.search(r"Detected flash size:\s*(\d+)(KB|MB)", probe.stdout)
            if not detected or int(detected[1]) * (1024 if detected[2] == "KB" else 1024 * 1024) < profile["flashBytes"]:
                raise ValueError("physical flash probe failed or board is smaller than selected profile")
            subprocess.run(command, check=True)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"flash rejected: {error}\n")

if __name__ == "__main__":
    main()
