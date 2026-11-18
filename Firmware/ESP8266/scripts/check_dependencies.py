"""Stop before compilation when the local dependency setup is absent or changed."""

import subprocess

Import("env")
result = subprocess.run(
    [
        env.subst("$PYTHONEXE"),
        env.subst("$PROJECT_DIR/scripts/configure_dependencies.py"),
        "--check",
    ]
)
if result.returncode:
    raise SystemExit(result.returncode)
