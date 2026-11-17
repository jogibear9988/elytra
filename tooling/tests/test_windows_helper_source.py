#!/usr/bin/env python3
"""Source/transaction controls only; these do not claim native Windows confinement acceptance."""

import _support

import hashlib
import os
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "windows_helper_composition", ROOT / "tooling/windows-sandbox/prepare.py"
)
composer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(composer)

stage_spec = importlib.util.spec_from_file_location(
    "windows_helper_stage", ROOT / "tooling/windows-sandbox/stage.py"
)
stager = importlib.util.module_from_spec(stage_spec)
stage_spec.loader.exec_module(stager)


class HelperSources(unittest.TestCase):
    def test_pins_reversal_and_ambiguity_refusal(self):
        for name, (relative, expected) in composer.PINS.items():
            raw = (ROOT / "vendor/ladybird" / relative).read_bytes()
            self.assertEqual(hashlib.sha256(raw).hexdigest(), expected)
            original = raw.decode()
            result = composer.compose(name, original)
            self.assertEqual(composer.reverse(name, result), original)
            self.assertIn("SPDX-License-Identifier: BSD-2-Clause", result)
            for before, _ in composer.changes(name):
                for invalid in (original.replace(before, "", 1), original + before, result):
                    with self.assertRaises(ValueError):
                        composer.compose(name, invalid)

    def test_drift_preserves_all_previous_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            for _, (relative, _) in composer.PINS.items():
                path = root / "vendor/ladybird" / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((ROOT / "vendor/ladybird" / relative).read_bytes())
            previous = composer.ROOT
            try:
                composer.ROOT = root
                output = root / "output"
                composer.prepare(output)
                accepted = {p.name: p.read_bytes() for p in output.iterdir()}
                relative = composer.PINS["Process.cpp"][0]
                path = root / "vendor/ladybird" / relative
                path.write_bytes(path.read_bytes() + b"\n")
                with self.assertRaises(ValueError):
                    composer.prepare(output)
                self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, accepted)
                absent = root / "absent"
                with self.assertRaises(ValueError):
                    composer.prepare(absent)
                self.assertFalse(absent.exists())
            finally:
                composer.ROOT = previous

    def test_layout_refuses_unselected_or_missing_native_authority(self):
        with tempfile.TemporaryDirectory() as directory:
            for selected in (False, True):
                script = Path(directory) / "layout.cmake"
                script.write_text(
                    "set(APPLE FALSE)\nset(WIN32 TRUE)\nset(CMAKE_SYSTEM_NAME Windows)\n"
                    + f"set(FIREFLY_EXPERIMENTAL_WINDOWS_HELPER_LAUNCH {'TRUE' if selected else 'FALSE'})\n"
                    + f'include("{(ROOT / "firefly/renderer-helper/Layout.cmake").as_posix()}")\n'
                )
                result = subprocess.run(
                    ["cmake", "-P", str(script)], capture_output=True, text=True
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "Native Windows helper launch authority is missing"
                    if selected
                    else "No verified native helper layout/sandbox adapter",
                    result.stderr,
                )

    def test_sealed_physical_stage_tamper_and_ownership(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            source = root / "producer"
            source.mkdir()
            binary = source / "producer.fixture"
            dll = source / "provider.dll"
            binary.write_bytes(b"source fixture: not an executable")
            dll.write_bytes(b"source fixture: not a DLL")
            resources = source / "resources"
            resources.mkdir()
            (resources / "resource.txt").write_bytes(b"real source fixture bytes")
            targets = source / "inputs.txt"
            targets.write_text(f"WebContent.exe|{binary}\n{dll}\n")
            revision = "1" * 40
            destination = root / "firefly-helper"
            stager.stage(destination, targets, resources, revision)
            sealed = stager.parse_seal(destination / stager.SEAL)
            self.assertEqual(
                set(sealed), {"bin/WebContent.exe", "bin/provider.dll", "share/Lagom/resource.txt"}
            )
            self.assertEqual((destination / "bin/WebContent.exe").read_bytes(), binary.read_bytes())
            finder = destination / ".DS_Store"
            finder.write_bytes(b"preserve Finder metadata")
            stager.stage(destination, targets, resources, revision)
            self.assertEqual(finder.read_bytes(), b"preserve Finder metadata")
            unknown = destination / "bin/private.txt"
            unknown.write_bytes(b"unowned bytes")
            before = {
                p.relative_to(destination): p.read_bytes()
                for p in destination.rglob("*")
                if p.is_file()
            }
            with self.assertRaises(ValueError):
                stager.stage(destination, targets, resources, revision)
            self.assertEqual(
                {
                    p.relative_to(destination): p.read_bytes()
                    for p in destination.rglob("*")
                    if p.is_file()
                },
                before,
            )
            unknown.unlink()
            (destination / "bin/provider.dll").write_bytes(b"tampered")
            with self.assertRaises(ValueError):
                stager.stage(destination, targets, resources, revision)
            unowned = root / "unowned"
            unowned.mkdir()
            with self.assertRaises(ValueError):
                stager.stage(unowned, targets, resources, revision)
            self.assertEqual(list(unowned.iterdir()), [])

    def test_hardlinks_and_reparse_paths_refused_before_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            outside = root / "outside.txt"
            outside.write_bytes(b"must never change")
            alias = root / "alias.txt"
            try:
                os.link(outside, alias)
            except OSError as error:
                self.skipTest(f"Host filesystem cannot create hard-link refusal fixture: {error}")
            with self.assertRaises(ValueError):
                stager.digest(alias)
            alias.unlink()
            try:
                alias.symlink_to(outside)
            except OSError as error:
                self.skipTest(f"Host cannot create symlink refusal fixture: {error}")
            with self.assertRaises(ValueError):
                stager.digest(alias)
            self.assertEqual(outside.read_bytes(), b"must never change")

    def test_original_create_handle_never_truncates_replacement_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            source = root / "producer"
            source.mkdir()
            binary = source / "image.fixture"
            binary.write_bytes(b"source-only image fixture")
            resources = source / "resources"
            resources.mkdir()
            (resources / "resource.txt").write_bytes(b"source-only resource fixture")
            targets = source / "inputs.txt"
            targets.write_text(f"WebContent.exe|{binary}\n")
            outside = root / "outside-private.txt"
            outside.write_bytes(b"external bytes must never be truncated")
            destination = root / "firefly-helper"
            original = stager.Creator.file
            blocked = []

            def replace_after_create(creator, path):
                output = original(creator, path)
                if path.name == stager.JOURNAL:
                    try:
                        path.unlink()
                    except PermissionError:
                        # Actual Windows CREATE_NEW handle denies deletion.
                        blocked.append(True)
                    else:
                        os.link(outside, path)
                return output

            with patch.object(stager.Creator, "file", replace_after_create):
                if os.name == "nt":
                    stager.stage(destination, targets, resources, "1" * 40)
                    self.assertEqual(blocked, [True])
                else:
                    # This host permits unlinking an open file. Writes still
                    # target the original file object; final identity checks
                    # refuse the foreign alias before a seal is published.
                    with self.assertRaises(ValueError):
                        stager.stage(destination, targets, resources, "1" * 40)
                    self.assertFalse((destination / stager.SEAL).exists())
            self.assertEqual(outside.read_bytes(), b"external bytes must never be truncated")


if __name__ == "__main__":
    unittest.main()
