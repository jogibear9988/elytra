#!/usr/bin/env python3
"""Verify generated native Firefly scripts without requiring Node in a native build."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    generated = ROOT / "firefly/core/generated"
    manifest = json.loads((generated / "manifest.json").read_text())
    expected = {}
    for name, digest in manifest["files"].items():
        if Path(name).name != name:
            raise SystemExit("Invalid generated Firefly filename")
        expected[generated / name] = digest
    for name, digest in manifest["sources"].items():
        file = ROOT / name
        if not file.resolve().is_relative_to(ROOT) or not name.startswith(
            ("firefly/", "tooling/runtime-js/")
        ):
            raise SystemExit("Invalid Firefly source path")
        expected[file] = digest
    for file, digest in expected.items():
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            raise SystemExit(
                f"Stale Firefly ESM adapter: {file.relative_to(ROOT)}; run node tooling/runtime-js/build-firefly.mjs"
            )
    print("Pinned Firefly ESM sources and generated adapters verified.")


if __name__ == "__main__":
    main()
