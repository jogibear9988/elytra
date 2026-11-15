#!/usr/bin/env python3
"""Check shared native app payload selection and unsupported-host packaging refusal without UI."""

import _support

import json
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

from native_package.payload import copy_application
from package_linux import stage
import package_macos


class AppPayload(unittest.TestCase):
    def fixture(self, source):
        source.mkdir()
        (source / "main.mjs").write_text("// fixture main\n")
        entries = {
            "": {},
            "node_modules/runtime": {"version": "1"},
            "node_modules/build-tool": {"version": "1", "dev": True},
        }
        for name, value in entries.items():
            if not name:
                continue
            directory = source / name
            directory.mkdir(parents=True)
            (directory / "package.json").write_text(json.dumps({"version": "1"}))
            (directory / "LICENSE").write_text("fixture notice")
        (source / "package-lock.json").write_text(
            json.dumps({"lockfileVersion": 3, "packages": entries})
        )

    def test_root_production_and_all_preserve_original(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            self.fixture(source)
            copy_application(source, root / "prod")
            self.assertTrue((root / "prod/node_modules/runtime/LICENSE").is_file())
            self.assertFalse((root / "prod/node_modules/build-tool").exists())
            copy_application(source, root / "all", "all")
            self.assertTrue((root / "all/node_modules/build-tool/LICENSE").is_file())
            self.assertTrue((source / "node_modules/build-tool/LICENSE").is_file())

    def test_bundled_designer_mode_and_plain_app(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            (source / "main.mjs").write_text("// fixture main\n")
            copy_application(source, root / "plain")
            (source / "designer-package-lock.json").write_text("{}")
            (source / "BUILD.json").write_text(
                json.dumps({"webAssets": "bundled", "dependencies": {"mode": "production"}})
            )
            copy_application(source, root / "bundled")
            with self.assertRaisesRegex(RuntimeError, "Rebuild bundled Designer"):
                copy_application(source, root / "wrong-mode", "all")
            self.assertFalse((root / "wrong-mode").exists())

    def test_lock_and_platform_refusal(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            self.fixture(source)
            (source / "package-lock.json").unlink()
            with self.assertRaisesRegex(ValueError, "require a package lock"):
                copy_application(source, root / "unlocked")
            self.assertFalse((root / "unlocked").exists())
            with patch("sys.platform", "darwin"):
                with self.assertRaisesRegex(RuntimeError, "requires native Linux x64"):
                    stage(root, source, root / "unsupported", "0.1.0-dev.1", "fixture")
            self.assertFalse((root / "unsupported").exists())


class MacHelperInventory(unittest.TestCase):
    def test_pinned_helpers_include_system_probe(self):
        source = (
            package_macos.ROOT / "vendor/ladybird/Meta/CMake/ladybird_helper_processes.cmake"
        ).read_text()
        names = set(re.search(r"set\(ladybird_helper_processes\s+(.*?)\)", source, re.S)[1].split())
        names.update(
            re.findall(r"list\(APPEND ladybird_helper_processes ([A-Za-z0-9_]+)\)", source)
        )
        self.assertIn("SystemProbe", package_macos.HELPERS)
        self.assertEqual(len(package_macos.HELPERS), len(set(package_macos.HELPERS)))
        self.assertEqual(names, set(package_macos.HELPERS))

    def test_system_probe_preserves_original_helper_security_metadata(self):
        source, destination = Path("/owned/build/helpers"), Path("/owned/package/helpers")
        seen = []

        def metadata(path):
            seen.append(path)
            return ({"upstream-entitlement": True}, "0x10000")

        with patch("package_macos.signing_state", side_effect=metadata):
            package_macos.check_helpers(source, destination)
        self.assertIn(source / "SystemProbe", seen)
        self.assertIn(destination / "SystemProbe", seen)

        def changed_metadata(path):
            if path == destination / "SystemProbe":
                return ({}, "0x0")
            return ({"upstream-entitlement": True}, "0x10000")

        with patch("package_macos.signing_state", side_effect=changed_metadata):
            with self.assertRaisesRegex(RuntimeError, "SystemProbe"):
                package_macos.check_helpers(source, destination)


if __name__ == "__main__":
    unittest.main()
