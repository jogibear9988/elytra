#!/usr/bin/env python3
"""Guard the Windows checkout regression without creating forbidden files on Windows."""

import _support  # Shared build-helper import path for standalone test CLIs.

from pathlib import Path
import subprocess
import tempfile
import unittest
from check_checkout_paths import check, reason, tracked_paths


class CheckoutPaths(unittest.TestCase):
    def test_device_names_in_files_and_directories(self):
        for name in [
            "CON",
            "prn",
            "AuX",
            "nul",
            "COM1",
            "com9",
            "LPT1",
            "lpt9",
            "COM¹",
            "COM²",
            "COM³",
            "LPT¹",
            "LPT²",
            "LPT³",
        ]:
            for path in [name, f"tests/{name}.mjs", f"tests/{name}/fixture.js", f"{name}.tar.gz"]:
                with self.subTest(path=path):
                    self.assertIsNotNone(reason(path))

    def test_invalid_characters_and_trailing_components(self):
        for char in '<>:"\\|?*' + "".join(chr(i) for i in range(32)):
            with self.subTest(char=repr(char)):
                self.assertIsNotNone(reason("tests/a" + char + "b.js"))
        for path in [
            "tests./ok.js",
            "tests/a.js ",
            "tests/a.js.",
            "../outside",
            "a/../b",
            "/absolute",
            "a//b",
        ]:
            with self.subTest(path=path):
                self.assertIsNotNone(reason(path))

    def test_valid_nearby_names(self):
        for path in [
            "mite/tests/dns/null-byte.mjs",
            "CONSOLE.js",
            "con-file.js",
            "com0.js",
            "com10.js",
            "lpt10/data",
            "null.mjs",
            ".github/workflows/windows.yml",
            "space in a directory/file.mjs",
            "unicode/é.mjs",
        ]:
            with self.subTest(path=path):
                self.assertIsNone(reason(path))

    def test_only_tracked_files_are_audited(self):
        with tempfile.TemporaryDirectory(prefix="checkout-guard-") as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "--quiet", str(root)], check=True)
            (root / "null-byte.mjs").write_text("// fixture\n")
            (root / "untracked.tmp").write_text("not an input\n")
            subprocess.run(["git", "-C", str(root), "add", "null-byte.mjs"], check=True)
            self.assertEqual(tracked_paths(root), ["null-byte.mjs"])
            self.assertEqual(check(tracked_paths(root)), [])

    def test_original_regression(self):
        self.assertEqual(
            check(["mite/tests/dns/nul.mjs"]),
            [("mite/tests/dns/nul.mjs", "reserved Windows device name")],
        )


if __name__ == "__main__":
    unittest.main()
