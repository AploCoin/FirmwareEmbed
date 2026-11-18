#!/usr/bin/env python3
"""Test real firmware sources with synthetic inputs and scripted SDK/board boundaries."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FLAGS = [
    "-std=c++11",
    "-Wall",
    "-Wextra",
    "-fsanitize=address,undefined",
    "-fno-omit-frame-pointer",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    projects = ROOT / "Firmware"
    boards = [
        path.name
        for path in sorted(projects.iterdir())
        if (path / "platformio.ini").is_file()
    ]
    parser.add_argument("--board", default="ESP32", choices=boards)
    project = projects / parser.parse_args().board
    dependency_script = project / "scripts/configure_dependencies.py"
    check = subprocess.run(
        [sys.executable, str(dependency_script), "--check"]
    )
    if check.returncode:
        return check.returncode
    subprocess.run(
        [sys.executable, str(ROOT / "tests/dependencies.py"), str(dependency_script)],
        check=True,
    )
    paths = json.loads((project / ".dependency-paths.json").read_text())
    base = Path(paths["AploEmbed"])
    mining = Path(paths["MiningEmbed"])
    compiler = os.environ.get("CXX", "c++")
    with tempfile.TemporaryDirectory(prefix="aplo-firmware-tests-") as directory:
        work = Path(directory)

        def compile_and_run(name, sources, includes, flags=()):
            binary = work / name
            subprocess.run(
                [
                    compiler,
                    *FLAGS,
                    *flags,
                    *["-I" + str(path) for path in includes],
                    *[str(path) for path in sources],
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)

        simple_includes = [ROOT / "tests/stubs", project / "include"]
        compile_and_run(
            "provisioning",
            [ROOT / "tests/provisioning.cpp", project / "src/SessionProvisioner.cpp"],
            simple_includes,
            ["-Werror"],
        )
        compile_and_run(
            "session-import",
            [
                ROOT / "tests/session_import.cpp",
                project / "src/SessionProvisioner.cpp",
                project / "src/SessionKeyImport.cpp",
                mining / "src/MiningWork.cpp",
            ],
            simple_includes + [ROOT / "tests/runtime_stubs", mining / "src"],
            ["-Werror"],
        )

        # Build the pinned integer implementation once for all native configuration/runtime cases.
        integer_objects = []
        for source in ("uint128_t.cpp", "uint256_t.cpp"):
            output = work / (source + ".o")
            subprocess.run(
                [
                    compiler,
                    *FLAGS,
                    "-I" + str(ROOT / "tests/stubs"),
                    "-I" + str(base / "src"),
                    "-c",
                    str(base / "src/uint256" / source),
                    "-o",
                    str(output),
                ],
                check=True,
            )
            integer_objects.append(output)
        compile_and_run(
            "config-values",
            [
                ROOT / "tests/config_values.cpp",
                project / "src/ConfigValues.cpp",
                *integer_objects,
            ],
            simple_includes + [base / "src"],
        )

        runtime_sources = [ROOT / "tests/runtime.cpp"] + [
            project / path
            for path in (
                "src/main.cpp",
                "src/DeviceSetup.cpp",
                "src/MiningApplication.cpp",
                "src/ConfigValues.cpp",
            )
        ]
        runtime_includes = [
            ROOT / "tests/runtime_stubs",
            project / "include",
            base / "src",
            mining / "src",
        ]
        modes = [
            ("missing", []),
            ("default-ca", ["-DAPLO_BUILD_VALIDATION=1"]),
            (
                "custom-ca",
                ["-DAPLO_BUILD_VALIDATION=1", '-DAPLO_RPC_ROOT_CA="synthetic-ca"'],
            ),
        ]
        for name, flags in modes:
            compile_and_run(
                name, runtime_sources + integer_objects, runtime_includes, flags
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
