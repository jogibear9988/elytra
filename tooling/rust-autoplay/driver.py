#!/usr/bin/env python3
"""Select the checked WebAudio crate root and preserve every other rustc input."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

CRATE = "libweb_bindings_rust"
KEYS = {"schema", "kind", "original", "adapted", "driver", "chain", "files"}


def command(config, compiler, arguments):
    if (
        set(config) != KEYS
        or config["schema"] != 1
        or config["kind"] != "firefly-rust-autoplay-driver"
    ):
        raise ValueError("Unknown WebAudio Rust driver configuration")
    own = Path(config["driver"]).resolve()
    if Path(compiler).resolve() == own or (
        config["chain"] and Path(config["chain"]).resolve() == own
    ):
        raise ValueError("WebAudio compiler driver cannot invoke itself")
    names = []
    for index, value in enumerate(arguments):
        if value == "--crate-name":
            if index + 1 == len(arguments) or arguments[index + 1].startswith("-"):
                raise ValueError("Missing Rust crate name")
            names.append(arguments[index + 1])
        elif value.startswith("--crate-name="):
            names.append(value.split("=", 1)[1])
    if len(names) > 1 or names == [""]:
        raise ValueError("Ambiguous Rust crate name")
    original = Path(config["original"]).resolve()
    adapted = Path(config["adapted"]).resolve()
    matches = [
        i
        for i, value in enumerate(arguments)
        if value and not value.startswith("-") and Path(value).resolve() == original
    ]
    result = list(arguments)
    if names == [CRATE]:
        if len(matches) != 1:
            raise ValueError("Review WebAudio Rust root input selection")
        for filename, expected in config["files"].items():
            try:
                actual = hashlib.sha256(Path(filename).read_bytes()).hexdigest()
            except OSError as error:
                raise ValueError(f"Missing WebAudio Rust input/output: {filename}") from error
            if actual != expected:
                raise ValueError(f"WebAudio Rust input/output drift: {filename}")
        result[matches[0]] = config["adapted"]
    elif matches or any(
        value and not value.startswith("-") and Path(value).resolve() == adapted
        for value in arguments
    ):
        raise ValueError("WebAudio Rust root passed with an unknown crate identity")
    prefix = [config["chain"]] if config["chain"] else []
    return [*prefix, compiler, *result]


def main():
    if len(sys.argv) < 3:
        raise ValueError("WebAudio driver requires configuration and real compiler")
    config = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    arguments = command(config, sys.argv[2], sys.argv[3:])
    if os.name == "nt":
        return subprocess.run(arguments).returncode
    os.execv(arguments[0], arguments)


if __name__ == "__main__":
    raise SystemExit(main())
