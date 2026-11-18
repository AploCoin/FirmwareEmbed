#!/usr/bin/env python3
"""Build all supported targets, keeping logs and validating dependency inputs."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = ROOT / "Firmware"


def check(project):
    subprocess.run(
        [sys.executable, str(project / "scripts/configure_dependencies.py"), "--check"],
        check=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", default="pio")
    args = parser.parse_args()
    logs = ROOT / "build-logs"
    logs.mkdir(exist_ok=True)
    results = []
    locks = {}
    for project in sorted(PROJECTS.iterdir()):
        if not (project / "platformio.ini").is_file():
            continue
        target = project.name
        check(project)
        locks[target] = json.loads((project / "dependencies.lock.json").read_text())
        log = logs / f"{target}.log"
        command = [args.pio, "run", "-d", str(project)]
        print("Building", target, "— log:", log, flush=True)
        with log.open("w") as output:
            run = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT)
        content = log.read_text(errors="replace")
        sizes = {}
        for label in ("RAM", "Flash"):
            match = re.search(
                rf"{label}:\s+.*?used (\d+) bytes from (\d+) bytes", content
            )
            if match:
                sizes[label.lower()] = {
                    "used": int(match[1]),
                    "available": int(match[2]),
                }
        result = {
            "target": target,
            "exitCode": run.returncode,
            "command": command,
            "log": str(log),
            **sizes,
        }
        results.append(result)
        print(json.dumps(result), flush=True)
        check(project)
    report = {
        "timestampUtc": datetime.now(timezone.utc).isoformat(),
        "dependencyLocks": locks,
        "platformioCoreDir": os.environ.get("PLATFORMIO_CORE_DIR"),
        "results": results,
    }
    (logs / "matrix.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if results and all(r["exitCode"] == 0 for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
