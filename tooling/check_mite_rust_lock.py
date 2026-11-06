#!/usr/bin/env python3
"""Reject aggregate Rust dependencies outside the pinned Ladybird lock inventory."""

import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def inventory(file):
    packages = set()
    for block in re.split(r"^\[\[package\]\]\s*$", Path(file).read_text(), flags=re.M)[1:]:
        fields = {}
        for key in ["name", "version", "source", "checksum"]:
            match = re.search(r"^" + key + r' = ("[^\n]*")\s*$', block, re.M)
            fields[key] = json.loads(match[1]) if match else None
        if not fields["name"] or not fields["version"]:
            raise ValueError("Malformed Cargo package record")
        packages.add(tuple(fields[key] for key in ["name", "version", "source", "checksum"]))
    return packages


def check(root=ROOT):
    allowed = inventory(root / "vendor/ladybird/Cargo.lock")
    selected = inventory(root / "mite/rust/Cargo.lock")
    own = ("mite_runtime_rust", "0.1.0", None, None)
    unexpected = selected - allowed - {own}
    if unexpected:
        raise ValueError(
            "Mite Rust dependencies drift from upstream lock: " + repr(sorted(unexpected))
        )
    if own not in selected:
        raise ValueError("Missing Mite aggregate lock entry")
    print(
        f"PASS aggregate Rust lock: {len(selected) - 1} pinned upstream packages; no new external versions"
    )


if __name__ == "__main__":
    check()
