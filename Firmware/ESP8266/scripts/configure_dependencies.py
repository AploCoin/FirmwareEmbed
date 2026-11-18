#!/usr/bin/env python3
"""Pin local library inputs and emit PlatformIO symlinks; never copies secrets."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def git(path, *args):
    return subprocess.check_output(
        ["git", "-C", str(path), *args], text=True, stderr=subprocess.DEVNULL
    ).strip()


def fingerprint(path):
    files = list((path / "src").rglob("*"))
    files += [path / "library.json", path / "library.properties"]
    files += [
        f for f in (path / "tools/trust").rglob("*") if "__pycache__" not in f.parts
    ]
    digest = hashlib.sha256()
    for file in sorted(f for f in files if f.is_file()):
        digest.update(file.relative_to(path).as_posix().encode() + b"\0")
        digest.update(file.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def load_json(path):
    if not path.is_file():
        raise ValueError(
            f"Missing {path.name}. Run scripts/configure_dependencies.py "
            "with --aploembed and --miningembed checkout paths first."
        )
    return json.loads(path.read_text())


def inspect_checkout(name, path):
    manifest = load_json(path / "library.json")
    if manifest.get("name") != name:
        raise ValueError(f"{path}: expected library {name}")
    revision = git(path, "rev-parse", "--verify", "HEAD^{commit}")
    if git(path, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError(
            f"{name}: checkout has uncommitted changes; commit or restore them first"
        )
    source_paths = [
        file.relative_to(path).as_posix()
        for file in (path / "src").rglob("*")
        if file.is_file()
    ]
    source_paths += [
        f.relative_to(path).as_posix()
        for f in (path / "tools/trust").rglob("*")
        if f.is_file() and "__pycache__" not in f.parts
    ]
    source_paths += [
        file
        for file in ("library.json", "library.properties")
        if (path / file).is_file()
    ]
    tracked = set(git(path, "ls-files", "-z").split("\0"))
    if not (path / "src").is_dir() or any(file not in tracked for file in source_paths):
        raise ValueError(
            f"{name}: library sources must exist and be committed, including ignored files"
        )
    return {"revision": revision, "sourceSha256": fingerprint(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aploembed", type=Path)
    parser.add_argument("--miningembed", type=Path)
    parser.add_argument(
        "--validation",
        action="store_true",
        help="Enable public fixture for complete application size checks",
    )
    parser.add_argument(
        "--record",
        action="store_true",
        help="Record a deliberately agreed revision/tree pair",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check previously configured paths without rewriting",
    )
    args = parser.parse_args()
    if args.check and (
        args.record or args.validation or args.aploembed or args.miningembed
    ):
        parser.error("--check cannot be combined with configuration/update options")
    paths_file = ROOT / ".dependency-paths.json"
    if args.check:
        paths = {k: Path(v) for k, v in load_json(paths_file).items()}
    else:
        if not args.aploembed or not args.miningembed:
            parser.error("specify --aploembed and --miningembed checkout paths")
        paths = {
            "AploEmbed": args.aploembed.resolve(),
            "MiningEmbed": args.miningembed.resolve(),
        }
    if set(paths) != {"AploEmbed", "MiningEmbed"}:
        raise ValueError(
            "Dependency paths must specify exactly AploEmbed and MiningEmbed"
        )
    records = {name: inspect_checkout(name, path) for name, path in paths.items()}
    lock_file = ROOT / "dependencies.lock.json"
    local_config = (
        "[local]\nlib_deps =\n"
        + "".join(f"    symlink://{p}\n" for p in paths.values())
        + "\n[firmware]\nbuild_flags = "
    )
    if args.check:
        local_file = ROOT / "platformio.local.ini"
        if not local_file.is_file():
            raise ValueError(
                "Missing platformio.local.ini. Run configure_dependencies.py again."
            )
        if local_file.read_text() not in (
            local_config + "\n",
            local_config + "-DAPLO_BUILD_VALIDATION=1\n",
        ):
            raise ValueError(
                "platformio.local.ini does not match the checked dependency paths; reconfigure"
            )
    if args.record:
        lock_file.write_text(json.dumps(records, indent=2) + "\n")
    elif records != load_json(lock_file):
        raise ValueError(
            "Dependency revision/source mismatch. Restore locked checkouts or "
            "review and explicitly --record new inputs."
        )
    if not args.check:
        paths_file.write_text(
            json.dumps({k: str(v) for k, v in paths.items()}, indent=2) + "\n"
        )
        (ROOT / "platformio.local.ini").write_text(
            local_config
            + ("-DAPLO_BUILD_VALIDATION=1" if args.validation else "")
            + "\n"
        )
    for name, record in records.items():
        print(
            f'{name}: {paths[name]} @ {record["revision"]}, '
            f'source SHA256 {record["sourceSha256"]}'
        )


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Dependency configuration error: {error}", file=sys.stderr)
        raise SystemExit(1)
