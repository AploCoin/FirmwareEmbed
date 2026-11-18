#!/usr/bin/env python3
"""Check dependency setup in independent clean repositories, without network."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

DEPENDENCY_SCRIPT = Path(sys.argv[1])


def run(command, success=True):
    result = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )
    assert (result.returncode == 0) == success, result.stdout
    assert "Traceback" not in result.stdout, result.stdout
    return result.stdout


with tempfile.TemporaryDirectory(prefix="firmware-dependency-test-") as directory:
    root = Path(directory)
    firmware = root / "firmware"
    (firmware / "scripts").mkdir(parents=True)
    script = firmware / "scripts/configure_dependencies.py"
    shutil.copy(DEPENDENCY_SCRIPT, script)
    command = [sys.executable, str(script)]
    assert "Missing .dependency-paths.json" in run(command + ["--check"], False)
    paths = []
    for name in ("AploEmbed", "MiningEmbed"):
        path = root / ("different library path " + name)
        (path / "src").mkdir(parents=True)
        (path / "src/source.cpp").write_text("int value = 1;\n")
        (path / "library.json").write_text('{"name": "' + name + '"}\n')
        run(["git", "init", "-q", str(path)])
        run(["git", "-C", str(path), "add", "."])
        run(
            [
                "git",
                "-C",
                str(path),
                "-c",
                "user.name=Fixture",
                "-c",
                "user.email=fixture@invalid",
                "commit",
                "-qm",
                "Synthetic fixture",
            ]
        )
        paths.append(path)
    configure = command + ["--aploembed", str(paths[0]), "--miningembed", str(paths[1])]
    run(configure + ["--record", "--validation"])
    lock = (firmware / "dependencies.lock.json").read_bytes()
    run(configure)
    run(command + ["--check"])
    assert lock == (firmware / "dependencies.lock.json").read_bytes()
    source = paths[0] / "src/source.cpp"
    source.write_text("int value = 2;\n")
    assert "uncommitted" in run(configure + ["--record"], False)
    run(command + ["--check"], False)
    run(["git", "-C", str(paths[0]), "add", "."])
    run(
        [
            "git",
            "-C",
            str(paths[0]),
            "-c",
            "user.name=Fixture",
            "-c",
            "user.email=fixture@invalid",
            "commit",
            "-qm",
            "Different revision",
        ]
    )
    assert "mismatch" in run(configure, False)
    run(["git", "-C", str(paths[0]), "checkout", "-q", "HEAD~1"])
    (paths[0] / "src/untracked.cpp").write_text("int extra;\n")
    run(configure + ["--record"], False)
    (paths[0] / "src/untracked.cpp").unlink()
    # A modified ignored source must not be accepted as a deliberate lock update.
    (paths[0] / ".git/info/exclude").write_text("src/ignored.cpp\n")
    (paths[0] / "src/ignored.cpp").write_text("int hidden;\n")
    assert "committed" in run(configure + ["--record"], False)
    (paths[0] / "src/ignored.cpp").unlink()
    run(command + ["--check"])
    assert lock == (firmware / "dependencies.lock.json").read_bytes()
    (firmware / "platformio.local.ini").unlink()
    assert "Missing platformio.local.ini" in run(command + ["--check"], False)
    print(
        "Dependency setup: relocated paths, missing config, dirty/untracked sources and revision lock PASS."
    )
