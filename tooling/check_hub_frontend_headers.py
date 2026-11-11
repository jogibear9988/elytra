#!/usr/bin/env python3
"""Reject mixed original/embedded RemoteView layouts in actual compiler commands."""

import argparse
import json
from pathlib import Path
import shlex

HEADER = Path("UI/Qt/RemoteView.h")


def command_arguments(entry):
    return entry.get("arguments") or shlex.split(entry["command"])


def selected_header(entry):
    """Match Clang/GCC angle-header lookup: -I paths precede all -isystem paths."""
    arguments = iter(command_arguments(entry))
    regular, system = [], []
    for argument in arguments:
        if argument in ("-I", "-isystem"):
            destination = regular if argument == "-I" else system
            destination.append(next(arguments))
        elif argument.startswith("-I"):
            regular.append(argument[2:])
        elif argument.startswith("-isystem"):
            system.append(argument[len("-isystem") :])
    directories = [(Path(entry["directory"]) / path).resolve() for path in regular + system]
    selected = next((path / HEADER for path in directories if (path / HEADER).is_file()), None)
    return selected, directories


def target_name(entry):
    output = entry.get("output")
    if output is None:
        arguments = command_arguments(entry)
        output = arguments[arguments.index("-o") + 1] if "-o" in arguments else ""
    parts = Path(output).parts
    for index, part in enumerate(parts[:-1]):
        if part == "CMakeFiles" and parts[index + 1].endswith(".dir"):
            return parts[index + 1].removesuffix(".dir")
    return None


def verify(entries, overlay, required_targets=(), upstream=None):
    overlay = overlay.resolve()
    expected = overlay / HEADER
    if not expected.is_file():
        raise ValueError("Missing composed RemoteView header: " + str(expected))
    originals = (
        {
            upstream.resolve() / "UI/Qt" / name
            for name in (
                "RemoteView.cpp",
                "WebContentView.cpp",
                "HubConnection.cpp",
                "ContextMenus.cpp",
            )
        }
        if upstream is not None
        else set()
    )
    covered = set()
    count = 0
    for entry in entries:
        selected, directories = selected_header(entry)
        target = target_name(entry)
        if overlay not in directories:
            if target in required_targets:
                raise ValueError(f"Missing Hub frontend include on {target}: {entry['file']}")
            continue
        if (Path(entry["directory"]) / entry["file"]).resolve() in originals:
            raise ValueError(
                "Standalone upstream frontend acquired embedded layout: " + entry["file"]
            )
        if selected != expected:
            raise ValueError(
                f"Mixed RemoteView ABI on {target}: {entry['file']} selects {selected}; expected {expected}"
            )
        covered.add(target)
        count += 1
    missing = set(required_targets) - covered
    if missing:
        raise ValueError(
            "No verified compile commands for Hub consumers: " + ", ".join(sorted(missing))
        )
    if count == 0:
        raise ValueError("No embedded Hub frontend compiler commands found")
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--overlay", required=True, type=Path)
    parser.add_argument("--require-target", action="append", default=[])
    parser.add_argument("--upstream", type=Path)
    args = parser.parse_args()
    count = verify(
        json.loads(args.database.read_text()), args.overlay, args.require_target, args.upstream
    )
    print(f"PASS identical embedded RemoteView header selected in {count} compiler commands")


if __name__ == "__main__":
    main()
