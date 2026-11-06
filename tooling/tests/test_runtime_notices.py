#!/usr/bin/env python3
"""Require distributed portable-runtime notices and reject tampered license bytes."""

import _support  # Standalone packaging-helper imports.

import json
from pathlib import Path
import shutil
import tempfile
from package_macos import copy_runtime_notices

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix="mite-runtime-notices-") as directory:
        root = Path(directory)
        source = root / "source"
        shutil.copytree(ROOT / "mite/modules/generated", source)
        destination = root / "valid"
        copy_runtime_notices(destination, source)
        manifest = json.loads((destination / "manifest.json").read_text())
        assert set(destination.iterdir()) == {destination / "manifest.json"} | {
            destination / (name + ".LICENSE.txt") for name in manifest["bundles"]
        }
        assert "event-target-shim 6.0.2" in (destination / "web-globals.LICENSE.txt").read_text()
        assert "web-streams-polyfill 4.5.1" in (destination / "web-globals.LICENSE.txt").read_text()
        notices = (destination / "fetch-data.LICENSE.txt").read_text()
        for package in [
            "fetch-blob 3.2.0",
            "busboy 1.6.0",
            "streamsearch 1.1.0",
            "whatwg-mimetype 4.0.0",
        ]:
            assert package in notices, package
        packages = manifest["bundles"]["fetch-data"]["packages"]
        assert {package["name"] for package in packages} == {
            "fetch-blob",
            "busboy",
            "streamsearch",
            "whatwg-mimetype",
        }
        assert all(package["license"] == "MIT" for package in packages)
        (source / "web-globals.LICENSE.txt").write_text("Removed copyright")
        try:
            copy_runtime_notices(root / "tampered", source)
        except ValueError:
            pass
        else:
            raise AssertionError("Tampered copyright was packaged")
    print("PASS portable dependency notices, manifest distribution and tamper refusal")


if __name__ == "__main__":
    main()
