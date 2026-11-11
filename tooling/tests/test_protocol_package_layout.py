#!/usr/bin/env python3
"""Check packaged-helper observer path policy without claiming Linux execution."""

import json
from pathlib import Path
import tempfile
import unittest

from test_protocol_configuration import package_layout


class PackageLayoutTests(unittest.TestCase):
    def test_mac_bundle_path_stays_exact(self):
        directory = Path("/physical/App.app/Contents/MacOS")
        self.assertEqual(
            package_layout(directory, "darwin"),
            (directory.parent / "Resources", directory.parent / "lib", None),
        )

    def test_linux_sidecar_manifest_and_root_libraries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            directory = root / "firefly-runtime/bin"
            directory.mkdir(parents=True)
            libraries = root / "lib"
            libraries.mkdir()
            library = libraries / "liblagom-web.so.0"
            library.touch()
            metadata = {
                "platform": "linux",
                "architecture": "x64",
                "elf": {
                    "platform": "linux-x64",
                    "binaries": ["firefly-runtime/bin/WebContent"],
                    "libraries": {library.name: {"sha256": "fixture-only"}},
                },
                "icuData": {"helpers": {"path": "firefly-runtime/share/Lagom/icu/icudt78l.dat"}},
            }
            manifest = root / "BUILD.json"
            manifest.write_text(json.dumps(metadata))
            self.assertEqual(
                package_layout(directory, "linux"),
                (root / "firefly-runtime/share/Lagom", libraries, {library}),
            )
            # A manifest must not redirect the observer to SDK resources/libraries.
            metadata["icuData"]["helpers"]["path"] = "sdk/share/Lagom/icu/icudt78l.dat"
            manifest.write_text(json.dumps(metadata))
            with self.assertRaises(AssertionError):
                package_layout(directory, "linux")
            metadata["icuData"]["helpers"]["path"] = "firefly-runtime/share/Lagom/icu/icudt78l.dat"
            metadata["elf"]["libraries"] = {"../sdk/liblagom-web.so.0": {}}
            manifest.write_text(json.dumps(metadata))
            with self.assertRaises(AssertionError):
                package_layout(directory, "linux")
            manifest.unlink()
            with self.assertRaises(AssertionError):
                package_layout(directory, "linux")

    def test_arbitrary_sdk_directory_is_not_linux_package(self):
        with self.assertRaises(AssertionError):
            package_layout(Path("/sdk/bin"), "linux")


if __name__ == "__main__":
    unittest.main()
