#!/usr/bin/env python3
"""ASan/UBSan checks of production storage/parser plus actual session/crypto code.

Only Arduino stream/clock and Web3 URL/X.509 boundaries use host doubles.
Neither flash drivers nor physical power interruption are simulated hardware evidence.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
APLO = ROOT.parent / "AploEmbed"
MINING = ROOT.parent / "MiningEmbed"
OUT = Path(tempfile.mkdtemp(prefix="firmware-storage-tests-"))
FLAGS = ["-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
         "-ffunction-sections", "-fdata-sections", "-DUSE_PRECOMPUTED_CP=0",
         "-I" + str(ROOT / "tests/storage/stubs"), "-I" + str(ROOT / "include"),
         "-I" + str(APLO / "src"), "-I" + str(MINING / "src")]
objects = []
sources = [APLO / f"src/trezor/{n}.c" for n in
           ["bignum", "ecdsa", "secp256k1", "sha3", "sha2", "hmac", "rfc6979", "memzero"]]
sources += [APLO / "src/session/SessionKey.cpp", APLO / "src/uint256/uint128_t.cpp",
            APLO / "src/uint256/uint256_t.cpp", MINING / "src/MiningEmbed.cpp", MINING / "src/MiningWork.cpp",
            ROOT / "src/DeviceConfig.cpp", ROOT / "src/SerialConfig.cpp", ROOT / "tests/storage/test.cpp"]
for i, source in enumerate(sources):
    target = OUT / f"{i}.o"
    flags = [] if source.suffix == ".c" else ["-std=c++11"]
    if source.is_relative_to(ROOT):
        flags += ["-Wall", "-Wextra", "-Werror"]
    subprocess.run(["cc" if source.suffix == ".c" else "c++", *FLAGS, *flags,
                    "-c", str(source), "-o", str(target)], check=True)
    objects.append(str(target))
binary = OUT / "storage-tests"
strip = "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections"
subprocess.run(["c++", "-fsanitize=address,undefined", *objects, strip, "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True, env={**os.environ, "ASAN_OPTIONS": "halt_on_error=1",
                                              "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
print("Native test binary:", binary)
