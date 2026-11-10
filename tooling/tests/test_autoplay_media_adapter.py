#!/usr/bin/env python3
"""Source-integrity controls for genuine media lifecycle/gesture compositions; no GUI."""

import _support

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from prepare_autoplay_media import DIGEST, PREFIX, ROOT, SEAMS, SOURCE, adapt, reverse


class MediaSourceComposition(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = (ROOT / "vendor/ladybird" / SOURCE).read_bytes().decode()

    def test_exact_pin_reversal_and_license(self):
        self.assertEqual(hashlib.sha256(self.original.encode()).hexdigest(), DIGEST)
        result = adapt(self.original)
        self.assertEqual(reverse(result), self.original)
        self.assertTrue(result[len(PREFIX) :].startswith(self.original.split("#include", 1)[0]))
        for _, replacement in SEAMS:
            self.assertEqual(result.count(replacement), 1)
        # Resource selection/pause and upstream allowlist/sandbox logic stay intact.
        for signature in ["load_element()", "pause_element()", "is_eligible_for_autoplay()"]:
            self.assertEqual(result.count(signature), self.original.count(signature))

    def test_every_missing_duplicate_or_changed_seam_is_rejected(self):
        for needle, _ in SEAMS:
            for invalid in [
                self.original.replace(needle, "", 1),
                self.original + needle,
                self.original.replace(needle, needle.replace("HTMLMediaElement", "Changed"), 1),
            ]:
                with self.assertRaises(ValueError):
                    adapt(invalid)

    def test_unrelated_drift_and_double_application_are_rejected(self):
        for invalid in [self.original + "\n", PREFIX + self.original, adapt(self.original)]:
            with self.assertRaises(ValueError):
                adapt(invalid)

    def test_invalid_input_never_creates_or_overwrites_output(self):
        with tempfile.TemporaryDirectory(prefix="autoplay-source-policy-") as directory:
            root = Path(directory)
            source = root / "changed.cpp"
            source.write_text(self.original + "\n")
            output = root / "output"
            command = [
                sys.executable,
                str(ROOT / "tooling/prepare_autoplay_media.py"),
                "--source",
                str(source),
                "--output-dir",
                str(output),
            ]
            for existing in [False, True]:
                if existing:
                    output.mkdir()
                    (output / "HTMLMediaElement.cpp").write_bytes(b"preserve")
                result = subprocess.run(command, capture_output=True, timeout=5)
                self.assertNotEqual(result.returncode, 0)
                if existing:
                    self.assertEqual((output / "HTMLMediaElement.cpp").read_bytes(), b"preserve")
                else:
                    self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
