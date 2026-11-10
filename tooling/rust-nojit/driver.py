#!/usr/bin/env python3
"""Select one checked Rust non-JIT composition; preserve every other rustc argument."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def command(config, compiler, arguments):
    if config.get("schema") != 1 or config.get("kind") != "mite-rust-nojit-driver":
        raise ValueError("Unknown Rust origin driver configuration")
    if str(Path(compiler).absolute()) == str(Path(config["driver"]).absolute()):
        raise ValueError("Compiler driver cannot invoke itself")
    names = [arguments[i + 1] for i, value in enumerate(arguments[:-1]) if value == "--crate-name"]
    names += [value.split("=", 1)[1] for value in arguments if value.startswith("--crate-name=")]
    if len(names) > 1:
        raise ValueError("Ambiguous Rust crate name")
    result = list(arguments)
    if names == ["libjs_rust"]:
        original = Path(config["original"]).resolve()
        matches = [
            i
            for i, value in enumerate(arguments)
            if not value.startswith("-") and Path(value).resolve() == original
        ]
        if len(matches) != 1:
            raise ValueError("Review Rust LibJS root input selection")
        for filename, expected in config["files"].items():
            if hashlib.sha256(Path(filename).read_bytes()).hexdigest() != expected:
                raise ValueError(f"Rust non-JIT adapter input/output drift: {filename}")
        result[matches[0]] = config["adapted"]
    prefix = [config["chain"]] if config.get("chain") else []
    if (
        config.get("chain")
        and Path(config["chain"]).absolute() == Path(config["driver"]).absolute()
    ):
        raise ValueError("Compiler cache chain cannot invoke the driver itself")
    return [*prefix, compiler, *result]


def main():
    config = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    arguments = command(config, sys.argv[2], sys.argv[3:])
    if os.name == "nt":
        return subprocess.run(arguments).returncode
    os.execv(arguments[0], arguments)


if __name__ == "__main__":
    raise SystemExit(main())
