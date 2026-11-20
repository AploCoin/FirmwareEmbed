#!/usr/bin/env python3
"""Validate and plan a USB firmware write; execute only with --execute (esptool 4.x)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import sys

from artifacts import SCHEMA, SECTOR, expected_esp32_map, lite_marker, partition_entries

CHIPS = {"ESP32": "esp32", "ESP32-C3": "esp32c3", "ESP32-S3": "esp32s3", "ESP8266": "esp8266"}
OPERATIONS = ("install", "update")


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


def lite_application(path, profile, offset):
    if path.is_symlink() or not path.is_file():
        raise ValueError("expected a regular Lite application file")
    if path.stat().st_size > profile["storage"]["offset"] - offset:
        raise ValueError("Lite application overlaps the persistent journal")
    raw = path.read_bytes()
    if lite_marker(profile["layout"]) not in raw:
        raise ValueError("Lite application belongs to a different flash layout")
    return raw


def plan(manifest_path, board, mode, info=None, lite_app=None, installed_app=None):
    if mode not in OPERATIONS:
        raise ValueError("expected Install or Update")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != SCHEMA or manifest.get("eraseAll") is not False or manifest.get("ota") is not False:
        raise ValueError(f"expected USB-only manifest schema {SCHEMA} with eraseAll=false")
    matches = [p for p in manifest["profiles"] if p["id"] == board]
    if len(matches) != 1:
        raise ValueError("select exactly one known board profile")
    profile = matches[0]
    edition = profile.get("edition")
    if profile["version"] != manifest["version"] or profile.get("ota") is not False or edition not in {"full", "lite"}:
        raise ValueError("inconsistent profile identity")
    flash = profile["flashBytes"]
    start, length = profile["storage"]["offset"], profile["storage"]["size"]
    if any(type(n) is not int for n in (flash, start, length)) or start <= 0 or length <= 0 or start % SECTOR or length % SECTOR or start + length > flash:
        raise ValueError("invalid preserved configuration range")
    application_offset = 0 if profile["chipFamily"] == "ESP8266" else 0x10000
    if edition == "lite":
        if info is not None or profile.get("templateBinary") is not True:
            raise ValueError("Lite uses build-time configuration")
        if mode == "update" and (installed_app is None or lite_app is None):
            raise ValueError("Lite Update requires the installed app and the new personal build")
        if installed_app is not None:
            if mode != "update":
                raise ValueError("the installed app is only used for Lite Update")
            lite_application(installed_app, profile, application_offset)
    elif lite_app is not None or installed_app is not None:
        raise ValueError("personal app files are only used for Lite")
    elif mode == "update":
        if not info:
            raise ValueError("update requires a fresh GET_INFO capture from the connected device")
        expected = {"board": board, "layout": profile["layout"], "profile_flash_bytes": flash,
                    "storage_offset": start, "storage_size": length}
        for key, value in expected.items():
            if str(info.get(key)) != str(value):
                raise ValueError(f"device {key} does not match target profile")
        if int(info.get("flash_bytes", 0)) < flash:
            raise ValueError("connected device has insufficient physical flash")
        if int(info.get("storage_schema", -1)) not in profile["readableStorageSchemas"]:
            raise ValueError("target cannot read current storage schema; rollback rejected")
    parts = profile[mode]
    if not parts or mode == "update" and len(parts) != 1:
        raise ValueError("update must contain exactly one application image")
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
        if lite_app is not None and offset == application_offset:
            personal = lite_application(lite_app, profile, offset)
            if personal == raw:
                raise ValueError("the stock Lite template is not a personal build")
            path, size = lite_app.resolve(), len(personal)
        end = offset + (size + SECTOR - 1) // SECTOR * SECTOR
        if end > flash or offset < start + length and end > start or any(offset < b and end > a for a, b in ranges):
            raise ValueError("sector erase/write crosses preserved storage, another part or physical flash")
        ranges.append((offset, end))
        validated.append((offset, path))
    return profile, validated


def validate_factory_table(raw, profile):
    region = profile["storage"]
    if partition_entries(raw) != expected_esp32_map(profile["edition"], region["offset"], region["size"]):
        raise ValueError("physical partition table differs from the target factory map")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--mode", choices=OPERATIONS, default="update")
    parser.add_argument("--device-info", type=Path, help="fresh configure.py info output saved to a file")
    parser.add_argument("--lite-app", type=Path, help="application built with your Lite DeviceConfig.h")
    parser.add_argument("--installed-app", type=Path, help="exact previous Lite binary retained locally")
    parser.add_argument("--port", required=True)
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    try:
        info = device_info(args.device_info.read_text()) if args.device_info else None
        profile, parts = plan(args.manifest, args.profile, args.mode, info, args.lite_app, args.installed_app)
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
            physical = int(detected[1]) * (1024 if detected[2] == "KB" else 1024 * 1024) if detected else 0
            if physical < profile["flashBytes"] or profile["edition"] == "lite" and physical != profile["flashBytes"]:
                raise ValueError("physical flash probe failed or board is smaller than selected profile")
            if args.mode == "update":
                with tempfile.TemporaryDirectory(prefix="firmwareembed-check-") as temporary:
                    if profile["chipFamily"] != "ESP8266":
                        table = Path(temporary) / "partitions.bin"
                        subprocess.run(base + ["read_flash", "0x8000", "0x1000", str(table)], check=True)
                        validate_factory_table(table.read_bytes(), profile)
                    if args.installed_app is not None:
                        previous = lite_application(args.installed_app, profile, parts[0][0])
                        current = Path(temporary) / "application.bin"
                        subprocess.run(base + ["read_flash", hex(parts[0][0]), hex(len(previous)), str(current)], check=True)
                        if hashlib.sha256(current.read_bytes()).digest() != hashlib.sha256(previous).digest():
                            raise ValueError("installed Lite application differs from the supplied binary")
            subprocess.run(command, check=True)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"flash rejected: {error}\n")

if __name__ == "__main__":
    main()
