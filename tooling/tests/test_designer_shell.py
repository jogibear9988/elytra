#!/usr/bin/env python3
"""Reject stale maintained Designer shell bytes before native packaging starts."""

import _support  # noqa: F401 - standalone shared-tool imports

import json
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from designer_shell import (
    EXAMPLE,
    SOURCE_FILES,
    apply_index_overlay,
    shell_metadata,
    verify_designer_shell,
)
from build_designer import source_changes
import package_linux
import package_macos


class DesignerShellTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="designer-shell-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.example = self.root / "owned"
        shutil.copytree(EXAMPLE, self.example)
        self.app = self.root / "app"
        self.app.mkdir()
        for target, source in SOURCE_FILES.items():
            file = self.app / target
            file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(self.example / source, file)
        (self.app / "web/index.html").write_text(apply_index_overlay("<head></head><nav></nav>"))
        self.metadata = {
            "runtime": "firefly",
            "webRoot": "web",
            "upstream": json.loads((self.example / "upstream.json").read_text()),
            "designerShell": shell_metadata(self.app, self.example),
        }
        self.write_metadata()

    def write_metadata(self):
        (self.app / "BUILD.json").write_text(json.dumps(self.metadata))

    def verify(self):
        return verify_designer_shell(self.app, example=self.example)

    def test_actual_owned_sources_roundtrip(self):
        self.assertEqual(self.verify(), self.metadata["designerShell"])
        self.assertIn("smoke.mjs", self.metadata["designerShell"]["files"])
        self.assertEqual(len(self.metadata["designerShell"]["files"]), 6)

    def test_every_stale_or_missing_shell_source_fails_even_with_rehashed_manifest(self):
        for target in SOURCE_FILES:
            with self.subTest(target=target):
                file = self.app / target
                original = file.read_bytes()
                file.write_bytes(original + b"\n// stale shell\n")
                self.metadata["designerShell"]["files"][target] = hashlib.sha256(
                    file.read_bytes()
                ).hexdigest()
                self.write_metadata()
                with self.assertRaises((ValueError, json.JSONDecodeError)):
                    self.verify()
                file.unlink()
                with self.assertRaises(ValueError):
                    self.verify()
                file.write_bytes(original)
                self.metadata["designerShell"] = shell_metadata(self.app, self.example)
                self.write_metadata()

    def test_source_update_invalidates_previous_staging(self):
        (self.example / "smoke.mjs").write_text("// maintained shell changed\n")
        with self.assertRaisesRegex(ValueError, "Stale Designer shell"):
            self.verify()

    def test_missing_stale_and_tampered_manifest_rejected(self):
        manifest = self.app / "BUILD.json"
        manifest.unlink()
        with self.assertRaisesRegex(ValueError, "BUILD.json is missing"):
            self.verify()
        self.metadata.pop("designerShell")
        self.write_metadata()
        with self.assertRaisesRegex(ValueError, "metadata is stale"):
            self.verify()
        self.metadata["designerShell"] = shell_metadata(self.app, self.example)
        self.metadata["designerShell"]["files"]["main.mjs"] = "0" * 64
        self.write_metadata()
        with self.assertRaisesRegex(ValueError, "metadata is stale"):
            self.verify()

    def test_index_overlay_and_final_index_bytes_are_checked(self):
        file = self.app / "web/index.html"
        original = file.read_text()
        file.write_text(original + "<!-- stale index -->")
        with self.assertRaisesRegex(ValueError, "metadata is stale"):
            self.verify()
        file.write_text(original.replace('id="window-close"', 'id="broken-close"'))
        with self.assertRaisesRegex(ValueError, "current owned toolbar overlay"):
            shell_metadata(self.app, self.example)
        with self.assertRaises(ValueError):
            apply_index_overlay(original)
        with self.assertRaises(ValueError):
            apply_index_overlay("<head></head><nav></nav><nav></nav>")

    def test_both_packagers_reject_before_output_creation(self):
        (self.app / "main.mjs").write_text("// old shell\n")
        destination = self.root / "package"
        with self.assertRaisesRegex(ValueError, "Stale Designer shell"):
            package_macos.stage_bundle(
                self.root,
                self.app,
                destination,
                "Test",
                "test.app",
                "1.0.0",
                self.root,
                "production",
                False,
            )
        self.assertFalse(destination.exists())
        with (
            patch.object(package_linux.elf, "native_tools"),
            patch.object(package_linux, "cache_value", return_value="ON"),
        ):
            with self.assertRaisesRegex(ValueError, "Stale Designer shell"):
                package_linux.stage(
                    self.root, self.app, destination, "1.0.0", "test", mite_build=self.root
                )
        self.assertFalse(destination.exists())

    def test_symlink_shell_file_is_not_a_verified_physical_copy(self):
        file = self.app / "main.mjs"
        file.unlink()
        file.symlink_to(self.example / "main.mjs")
        with self.assertRaisesRegex(ValueError, "physical file"):
            self.verify()

    def test_generic_application_does_not_need_designer_manifest(self):
        other = self.root / "other"
        other.mkdir()
        self.assertIsNone(verify_designer_shell(other))
        with self.assertRaises(ValueError):
            verify_designer_shell(other, required=True)

    def test_generic_non_object_upstream_metadata_is_not_designer(self):
        other = self.root / "generic"
        other.mkdir()
        for value in [None, "unrelated upstream", ["unrelated"]]:
            with self.subTest(value=value):
                (other / "BUILD.json").write_text(json.dumps({"upstream": value}))
                (other / "upstream.json").write_text(json.dumps(value))
                self.assertIsNone(verify_designer_shell(other))
                with self.assertRaises(ValueError):
                    verify_designer_shell(other, required=True)


class SourceDirtyTests(unittest.TestCase):
    def test_only_finder_metadata_is_excluded_without_changing_it(self):
        with tempfile.TemporaryDirectory(prefix="designer-source-status-") as temporary:
            root = Path(temporary)

            def git(*args):
                return subprocess.run(["git", *args], cwd=root, check=True, capture_output=True)

            git("init")
            (root / ".DS_Store").write_bytes(b"tracked metadata")
            (root / "source.js").write_text("original")
            git("add", ".")
            git(
                "-c",
                "user.name=Fixture",
                "-c",
                "user.email=fixture@example.invalid",
                "commit",
                "-m",
                "fixture",
            )
            (root / ".DS_Store").write_bytes(b"preserved modified metadata")
            (root / "nested").mkdir()
            (root / "nested/.DS_Store").write_bytes(b"untracked metadata")
            self.assertEqual(source_changes(root), "")
            self.assertEqual((root / ".DS_Store").read_bytes(), b"preserved modified metadata")
            (root / "source.js").write_text("changed")
            self.assertIn("source.js", source_changes(root))
            (root / "unknown.mjs").write_text("untracked source")
            self.assertIn("unknown.mjs", source_changes(root))


if __name__ == "__main__":
    unittest.main()
