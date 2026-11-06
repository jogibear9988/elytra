#!/usr/bin/env python3
"""Verify checked-in portable runtime bundles without requiring Node/npm to build Mite."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    generated = ROOT / "mite/modules/generated"
    build = ROOT / "tooling/runtime-js"
    manifest = json.loads((generated / "manifest.json").read_text())
    expected = {
        build / "build.mjs": manifest["builderSha256"],
        build / "package-lock.json": manifest["lockSha256"],
    }
    for name, value in manifest["files"].items():
        if Path(name).name != name:
            raise SystemExit("Invalid generated runtime filename")
        expected[generated / name] = value
    for bundle in manifest["bundles"].values():
        for source in bundle["sources"]:
            if not source["path"].startswith("node_modules/"):
                path = build / source["path"]
                if not path.resolve().is_relative_to(build):
                    raise SystemExit("Invalid runtime source path")
                expected[path] = source["sha256"]
    for name, value in manifest.get("sources", {}).items():
        path = ROOT / name
        if not path.resolve().is_relative_to(ROOT) or not name.startswith(
            ("mite/modules/", "mite/runtime/bootstrap/", "tooling/runtime-js/")
        ):
            raise SystemExit("Invalid builtin source path")
        expected[path] = value
    for path, value in expected.items():
        if not path.is_file() or digest(path) != value:
            raise SystemExit(
                f"Runtime JS artifact changed: {path.relative_to(ROOT)}. Regenerate with tooling/runtime-js/build.mjs."
            )
    print("Pinned runtime JS bundles, notices and adapter inputs verified.")


if __name__ == "__main__":
    main()
