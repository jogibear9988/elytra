"""Explicit C/C++ compiler-cache selection and verification of actual generated Ninja commands."""

from pathlib import Path
import shlex
import shutil
import subprocess


def arguments(enabled):
    program = shutil.which("ccache") if enabled else ""
    if enabled and not program:
        raise ValueError("Requested compiler cache requires ccache on PATH")
    # Clear cached launchers when opting out in an existing build directory.
    result = [
        f"-DENABLE_LAGOM_CCACHE={'ON' if enabled else 'OFF'}",
        f"-DCMAKE_C_COMPILER_LAUNCHER={program}",
        f"-DCMAKE_CXX_COMPILER_LAUNCHER={program}",
    ]
    if enabled:
        result.append(f"-DCCACHE_PROGRAM={program}")
    return result


def check(build, target):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if not line.startswith(("#", "//")) and ":" in line and "=" in line:
            name, value = line.split("=", 1)
            cache[name.split(":", 1)[0]] = value
    program = cache.get("CCACHE_PROGRAM", "")
    if cache.get("ENABLE_LAGOM_CCACHE") != "ON" or not Path(program).is_file():
        raise ValueError("Requested compiler cache is not configured")
    if any(
        cache.get(f"CMAKE_{language}_COMPILER_LAUNCHER") != program for language in ["C", "CXX"]
    ):
        raise ValueError("Requested compiler launcher was changed or ignored")
    compilers = {
        str(Path(cache[f"CMAKE_{language}_COMPILER"]).resolve()) for language in ["C", "CXX"]
    }
    commands = subprocess.check_output(
        ["ninja", "-C", str(build), "-t", "commands", target], text=True
    )
    count = 0
    for line in commands.splitlines():
        args = shlex.split(line)
        if "-c" not in args:
            continue
        for index, arg in enumerate(args):
            if arg.startswith("/") and str(Path(arg).resolve()) in compilers:
                if index == 0 or Path(args[index - 1]).resolve() != Path(program).resolve():
                    raise ValueError(f"Actual C/C++ command bypasses the requested cache: {line}")
                count += 1
                break
    if not count:
        raise ValueError("No actual cached C/C++ compile commands were checked")
    print(f"PASS compiler cache in {count} actual generated C/C++ commands for {target}")
