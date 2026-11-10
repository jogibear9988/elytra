#!/usr/bin/env python3
"""Verify generated Linux x64 C++ and Cargo commands use the portable x86-64-v2 baseline."""

import argparse
import hashlib
import json
from pathlib import Path
import shlex


def check(build):
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    if "ENABLE_CI_BASELINE_CPU:BOOL=ON" not in cache.splitlines():
        raise ValueError("Linux x64 distribution baseline is not enabled")
    cpp = 0
    for record in json.loads((build / "compile_commands.json").read_text(encoding="utf-8")):
        if Path(record["file"]).suffix not in [".cpp", ".cc", ".cxx", ".C"]:
            continue
        args = record.get("arguments") or shlex.split(record["command"])
        cpu = [arg for arg in args if arg.startswith(("-mcpu=", "-march="))]
        if cpu != ["-march=x86-64-v2"]:
            raise ValueError(f"Unexpected project CPU options for {record['file']}: {cpu}")
        cpp += 1
    # The owned origin composition adds one explicit Cargo freshness input.
    # Compare its actual generated root hash; never permit arbitrary extra flags.
    origin = build / "firefly-adapters/rust-origin/lib.rs"
    composition = []
    if origin.is_file():
        composition = [
            f"--cfg=firefly_origin_build_{hashlib.sha256(origin.read_bytes()).hexdigest()}"
        ]
    # The non-JIT adapter is installed after origin composition in the renderer,
    # and by itself in standalone Mite. Accept only its actual generated root hash.
    nojit = build / "mite-adapters/rust-nojit/lib.rs"
    if nojit.is_file():
        composition.append(
            f"--cfg=mite_nojit_build_{hashlib.sha256(nojit.read_bytes()).hexdigest()}"
        )
    rust = 0
    prefix = "CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_RUSTFLAGS="
    for line in (build / "build.ninja").read_text(encoding="utf-8").splitlines():
        if prefix not in line:
            continue
        # Ninja's escaped spaces/dollars occur inside CMake's shell command.
        args = shlex.split(line.replace("$$", "$").replace("$ ", " "))
        flags = [arg[len(prefix) :] for arg in args if arg.startswith(prefix)]
        expected = ["-Dwarnings", "-Ctarget-cpu=x86-64-v2"]
        # Pinned upstream retains frame pointers on Linux. They change unwind
        # diagnostics, not the CPU ISA; accept only that exact optional flag.
        accepted = [
            [*expected, *composition],
            [*expected, "-Cforce-frame-pointers=yes", *composition],
        ]
        if len(flags) != 1 or shlex.split(flags[0]) not in accepted:
            raise ValueError(f"Unexpected generated Cargo CPU/warning flags: {flags}")
        rust += 1
    if not cpp or not rust:
        raise ValueError(f"Missing generated baseline commands: C++={cpp}, Cargo={rust}")
    print(
        f"PASS Linux x86-64-v2 CPU baseline: {cpp} project C++ and {rust} Cargo commands; Rust warnings remain fatal"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    check(parser.parse_args().build)
