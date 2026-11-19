# Raises ESP8266 stacks before the core is compiled.
# Certificate verification fills the stock BearSSL side stack and the 4KB
# loop stack, so a watchdog interrupt there overwrites a canary. The loop
# stack moves to BSS so it can grow without consuming the system stack.

Import("env")

from pathlib import Path

root = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif8266")) / "cores" / "esp8266"

def replace_once(path, old, new, label):
    text = path.read_text()
    if old in text:
        path.write_text(text.replace(old, new, 1))
        print(label)
    elif new not in text:
        raise SystemExit("unexpected contents in %s" % path)

replace_once(
    root / "StackThunk.cpp",
    "#define _stackSize (6200/4)",
    "#define _stackSize (8192/4) /* AploEmbed: room for the TLS watchdog interrupt */",
    "BearSSL side stack set to 8192 bytes",
)
replace_once(
    root / "cont.h",
    "#define CONT_STACKSIZE 4096",
    "#define CONT_STACKSIZE 8192 /* AploEmbed: room for the TLS watchdog interrupt */",
    "loop stack set to 8192 bytes",
)
replace_once(
    root / "core_esp8266_main.cpp",
    "    cont_t s_cont __attribute__((aligned(16)));",
    "    static cont_t s_cont __attribute__((aligned(16))); /* AploEmbed: BSS, not the system stack */",
    "loop stack moved to BSS",
)
