#!/usr/bin/env python3
"""Refuse stale Cargo archives whose emitted dependencies omit the origin composition."""

import argparse
import hashlib
import json
from pathlib import Path


def audit(config_file, depfiles):
    config = json.loads(config_file.read_text(encoding="utf-8"))
    generated = Path(config["adapted"]).parent
    required = [generated / name for name in ("lib.rs", "origin.rs", "ffi.rs")]
    required += [Path(name) for name in config["files"] if Path(name).name == "origin_policy.rs"]
    assert len(required) == 4, "Missing owned Rust policy dependency"
    for path in required:
        assert hashlib.sha256(path.read_bytes()).hexdigest() == config["files"][str(path)], path
    for depfile in depfiles:
        dependencies = (
            depfile.read_text(encoding="utf-8")
            .replace("\\\n", "")
            .replace("\\ ", " ")
            .replace("\\", "/")
        )
        for path in required:
            assert str(path).replace("\\", "/") in dependencies, (
                f"Stale Rust archive {depfile}: missing {path}"
            )
    print(
        "PASS actual Cargo dependency outputs contain generated root/origin/FFI module and owned policy"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--depfile", type=Path, action="append", required=True)
    args = parser.parse_args()
    audit(args.config, args.depfile)


if __name__ == "__main__":
    main()
