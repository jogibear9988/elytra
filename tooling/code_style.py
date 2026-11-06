#!/usr/bin/env python3
"""Apply or check Elytra's pinned formatters on maintained first-party sources."""

import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "build/code-style-venv/bin"


def sources(requested):
    """Keep generated/vendor bytes outside source-authoring policy."""
    names = requested or subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT,
    ).decode().split("\0")
    return sorted(
        {
            name
            for name in names
            if name
            and (ROOT / name).is_file()
            and not name.startswith(("vendor/", "build/"))
            and "/node_modules/" not in name
            and "/generated/" not in name
        }
    )


def format_native(files, check):
    spec = importlib.util.spec_from_file_location(
        "native_style", ROOT / "tooling/code-style/native.py"
    )
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    executable = TOOLS / "clang-format"
    version = subprocess.check_output([str(executable), "--version"], text=True)
    if "21.1.2" not in version:
        raise RuntimeError("Install pinned tooling/code-style/requirements.txt")
    changed = []
    for name in files:
        file = ROOT / name
        before = file.read_text(encoding="utf-8")
        formatted = subprocess.check_output(
            [str(executable), str(file)], text=True, encoding="utf-8"
        )
        after = native.definition_spacing(formatted, file.suffix == ".h")
        if before != after:
            changed.append(name)
            if not check:
                with file.open("w", encoding="utf-8", newline="\n") as output:
                    output.write(after)
    if check and changed:
        print("Native files need formatting:", ", ".join(changed[:10]), file=sys.stderr)
    print(f"Native: {len(files)} checked, {len(changed)} {'need changes' if check else 'updated'}")
    return not (check and changed)


def format_cmake(files, check):
    """Keep native build descriptions readable without modifying generated build trees."""
    executable = TOOLS / "cmake-format"
    version = subprocess.check_output([str(executable), "--version"], text=True).strip()
    if version != "0.6.13":
        raise RuntimeError("Install pinned tooling/code-style/requirements.txt")
    changed = []
    for name in files:
        file = ROOT / name
        original = file.read_text(encoding="utf-8")
        formatted = subprocess.check_output(
            [str(executable), str(file)], text=True, encoding="utf-8"
        )
        if original != formatted:
            changed.append(name)
            if not check:
                with file.open("w", encoding="utf-8", newline="\n") as output:
                    output.write(formatted)
    if check and changed:
        print("CMake files need formatting:", ", ".join(changed[:10]), file=sys.stderr)
    print(f"CMake: {len(files)} checked, {len(changed)} {'need changes' if check else 'updated'}")
    return not (check and changed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--native-only", action="store_true")
    selection.add_argument("--python-only", action="store_true")
    selection.add_argument("--js-only", action="store_true")
    parser.add_argument("paths", nargs="*")
    args = parser.parse_args()
    files = sources(args.paths)
    passed = True
    if not (args.python_only or args.js_only):
        selected = [name for name in files if Path(name).suffix in [".cpp", ".h", ".c"]]
        if selected:
            passed = format_native(selected, args.check) and passed
        selected = [
            name for name in files if name.endswith(".cmake") or Path(name).name == "CMakeLists.txt"
        ]
        if selected:
            passed = format_cmake(selected, args.check) and passed
        selected = [name for name in files if name.endswith(".rs")]
        if selected:
            command = ["rustfmt", *(["--check"] if args.check else []), *selected]
            passed = subprocess.run(command, cwd=ROOT).returncode == 0 and passed
    if not (args.native_only or args.js_only):
        selected = [name for name in files if name.endswith(".py")]
        if selected:
            executable = TOOLS / "ruff"
            version = subprocess.check_output([str(executable), "--version"], text=True).strip()
            if version != "ruff 0.14.3":
                raise RuntimeError("Install pinned tooling/code-style/requirements.txt")
            command = [
                str(executable),
                "format",
                *(["--check"] if args.check else []),
                *selected,
            ]
            passed = subprocess.run(command, cwd=ROOT).returncode == 0 and passed
    if not (args.native_only or args.python_only):
        command = [
            "node",
            "tooling/code-style/format.mjs",
            *(["--check"] if args.check else []),
            *files,
        ]
        passed = subprocess.run(command, cwd=ROOT).returncode == 0 and passed
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
