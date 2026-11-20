"""Standalone PlatformIO identity. Release CI supplies APLO_RELEASE_VERSION."""
Import("env")
import os
import json
from pathlib import Path
import re

project = env.GetProjectConfig()
board = env["PIOENV"]
release = dict(project.items("release"))
if project.has_section("release:" + board):
    release.update(dict(project.items("release:" + board)))
version = os.environ.get("APLO_RELEASE_VERSION", "dev")
if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,63}", version):
    raise ValueError("invalid APLO_RELEASE_VERSION")
env.Append(CPPDEFINES=[
    ("APLO_FIRMWARE_VERSION", env.StringifyMacro(version)),
    ("APLO_BOARD_ID", env.StringifyMacro(board)),
    ("APLO_FLASH_LAYOUT", env.StringifyMacro(release["layout"])),
    ("APLO_FLASH_BYTES", int(release["flash_bytes"], 0)),
    ("APLO_STORAGE_OFFSET", int(release["storage_offset"], 0)),
    ("APLO_STORAGE_SIZE", int(release["storage_size"], 0)),
])

build_dir = Path(env.subst("$BUILD_DIR"))
build_dir.mkdir(parents=True, exist_ok=True)
(build_dir / "identity.json").write_text(json.dumps({
    "board": board, "version": version, "layout": release["layout"],
    "flashBytes": int(release["flash_bytes"], 0),
    "storage": {"offset": int(release["storage_offset"], 0), "size": int(release["storage_size"], 0)},
}, sort_keys=True) + "\n")
