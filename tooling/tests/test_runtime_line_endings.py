#!/usr/bin/env python3
"""Verify Git's Windows checkout conversion preserves the integrity-checked bytes."""

import _support  # Shared build-helper import path for standalone test CLIs.

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    manifest = json.loads((ROOT / "mite/modules/generated/manifest.json").read_text())
    expected = {
        "tooling/runtime-js/build.mjs": manifest["builderSha256"],
        "tooling/runtime-js/package-lock.json": manifest["lockSha256"],
    }
    expected.update(
        {"mite/modules/generated/" + name: digest for name, digest in manifest["files"].items()}
    )
    for bundle in manifest["bundles"].values():
        for source in bundle["sources"]:
            if not source["path"].startswith("node_modules/"):
                expected["tooling/runtime-js/" + source["path"]] = source["sha256"]
    expected.update(manifest.get("sources", {}))
    attributes = subprocess.check_output(
        ["git", "check-attr", "eol", "--", *expected], cwd=ROOT, text=True
    )
    assert all(line.endswith(": eol: lf") for line in attributes.splitlines()), attributes
    with tempfile.TemporaryDirectory(prefix="mite-windows-checkout-") as directory:
        subprocess.run(
            [
                "git",
                "-c",
                "core.autocrlf=true",
                "checkout-index",
                "--prefix=" + directory + "/",
                "--stdin",
                "-z",
            ],
            cwd=ROOT,
            # Binary NUL-delimited transport avoids Python CRLF conversion on Windows.
            input=("\0".join(expected) + "\0").encode("utf-8"),
            check=True,
        )
        for name, digest in expected.items():
            assert hashlib.sha256((Path(directory) / name).read_bytes()).hexdigest() == digest, name
    print("PASS core.autocrlf=true checkout preserves all integrity-protected runtime bytes")


if __name__ == "__main__":
    main()
