"""Use the pinned SDK build tool, never copy CA/runtime code into firmware."""

import json
from pathlib import Path

Import("env")
paths = json.loads(Path(env.subst("$PROJECT_DIR/.dependency-paths.json")).read_text())
script = Path(paths["AploEmbed"]) / "tools/trust/platformio.py"
if not script.is_file():
    raise SystemExit(
        "Pinned AploEmbed lacks trust profile tool; update dependencies.lock.json"
    )
env.SConscript(str(script), exports="env")
