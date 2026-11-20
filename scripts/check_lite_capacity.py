#!/usr/bin/env python3
"""Compile both modes with a public CA and synthetic values in a CI project copy."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

from artifacts import measure, read_project


def fixture(example, mode, certificate):
    strings = {
        "kSsid": '"Synthetic capacity fixture"',
        "kPassword": '"' + "p" * 63 + '"',
        "kPrimaryUrl": '"https://capacity.invalid:443/rpc"',
        "kChainId": '"55119"',
        "kOwner": '"0x1111111111111111111111111111111111111111"',
        "kSigner": '"0x7e5f4552091a69125d5dfcb7b8c2659029395bdf"',
        "kGaploBudget": '"1000000000000000000"',
        "kPrimaryCa": 'R"APLOCA(' + certificate + ')APLOCA"',
    }
    replacements = {
        "kConfigured": "true", "kMode": f"Mode::{mode}",
        "kExpiry": "1000", "kPrivateKey": "{" + "0, " * 31 + "1}",
        **strings,
    }
    for name, value in replacements.items():
        example, count = re.subn(rf"(\b{name}(?:\[\d*\])?(?: PROGMEM)? = )[^;\n]+;",
                                 lambda match: match[1] + value + ";", example)
        if count != 1:
            raise ValueError(f"missing or duplicated Lite configuration field {name}")
    return example


def check(project, environment, certificate, output):
    profile = read_project(project, environment)
    if profile.edition != "lite" or not profile.supported:
        raise ValueError("expected a publishable Lite profile")
    binary = project / ".pio/build" / environment / "firmware.bin"
    identity = binary.with_name("identity.json")
    header = project / "include/DeviceConfig.private.h"
    example = (project / "include/DeviceConfig.example.h").read_text()
    command = ["pio", "run", "-d", str(project), "-e", environment]
    if header.exists():
        raise FileExistsError("capacity check requires an isolated project without a private header")
    results = {}
    with header.open("x"):
        pass
    try:
        for mode in ("Session", "Direct"):
            header.write_text(fixture(example, mode, certificate))
            subprocess.run([*command, "-t", "clean"], check=True, stdout=subprocess.DEVNULL)
            with tempfile.TemporaryDirectory(prefix="lite-capacity-") as logs:
                log = Path(logs) / "build.log"
                with log.open("w") as stream:
                    build = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
                if build.returncode:
                    print(log.read_text()[-9000:])
                    build.check_returncode()
                resources = measure(log, profile)
            if json.loads(identity.read_text()).get("templateBinary") is not False:
                raise ValueError("personal build did not see its private fixture header")
            length = binary.stat().st_size
            start = 0 if profile.chip == "ESP8266" else 0x10000
            if start + (length + 4095) // 4096 * 4096 > profile.config_offset:
                raise ValueError("personal image would overlap the signing guard")
            results[mode.lower()] = {"binaryBytes": length, "staticRamBytes": resources["ram"]["used"]}
            print(f"{environment} {mode}: {length} bytes, static RAM {resources['ram']['used']} bytes")
    finally:
        header.unlink()
    asset = output / f"{profile.id}.json"
    result = json.loads(asset.read_text())
    result["configuredBuilds"] = {"testCaBytes": len(certificate), **results}
    asset.write_text(json.dumps(result, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--ca", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    certificate = args.ca.read_text(encoding="ascii")
    if not certificate.startswith("-----BEGIN CERTIFICATE-----") or len(certificate) > 2048:
        raise ValueError("expected the representative root CA PEM (at most 2 KiB)")
    check(args.project, args.environment, certificate, args.output)


if __name__ == "__main__":
    main()
