#!/usr/bin/env python3
"""Exercise upstream-pin guard rejection using isolated Git repositories."""

import _support  # Shared build-helper import path for standalone test CLIs.

import json
from pathlib import Path
import tempfile
import unittest
from check_upstream import check, git


class UpstreamGuard(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="mite-upstream-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        git(self.root, "init", "--quiet")
        self.upstream = self.root / "vendor/ladybird"
        self.upstream.mkdir(parents=True)
        git(self.upstream, "init", "--quiet")
        self.source = self.upstream / "source.cpp"
        self.source.write_text("// original\n")
        git(self.upstream, "add", "source.cpp")
        git(
            self.upstream,
            "-c",
            "user.name=Mite test",
            "-c",
            "user.email=fixture@example.invalid",
            "-c",
            "commit.gpgsign=false",
            "commit",
            "--quiet",
            "-m",
            "fixture",
        )
        self.revision = git(self.upstream, "rev-parse", "HEAD")
        self.manifest = self.root / "vendor/ladybird.json"
        self.pin(self.revision)
        git(
            self.root,
            "update-index",
            "--add",
            "--cacheinfo",
            f"160000,{self.revision},vendor/ladybird",
        )

    def pin(self, revision):
        self.manifest.write_text(json.dumps({"revision": revision}))

    def test_clean_pin(self):
        self.assertEqual(check(self.root), self.revision)

    def test_modified_tracked_source(self):
        self.source.write_text("// local patch\n")
        with self.assertRaisesRegex(ValueError, "source tree is modified"):
            check(self.root)
        git(self.upstream, "add", "source.cpp")
        with self.assertRaisesRegex(ValueError, "source tree is modified"):
            check(self.root)

    def test_untracked_source(self):
        (self.upstream / "extra.cpp").write_text("// local addition\n")
        with self.assertRaisesRegex(ValueError, "source tree is modified"):
            check(self.root)

    def test_manifest_mismatch(self):
        self.pin("1" * 40)
        with self.assertRaisesRegex(ValueError, "indexed submodule pin differ"):
            check(self.root)

    def test_checkout_mismatch(self):
        self.pin("1" * 40)
        git(self.root, "update-index", "--cacheinfo", f"160000,{'1' * 40},vendor/ladybird")
        with self.assertRaisesRegex(ValueError, "checkout differs"):
            check(self.root)

    def test_bad_manifest(self):
        self.pin("master")
        with self.assertRaisesRegex(ValueError, "full commit ID"):
            check(self.root)

    def test_uninitialized_checkout(self):
        self.source.unlink()
        # Git objects are read-only on Windows. Removing repository discovery is
        # sufficient; TemporaryDirectory handles final cleanup of its own fixture.
        (self.upstream / ".git").rename(self.root / "removed-upstream-git")
        with self.assertRaisesRegex(ValueError, "Initialize Ladybird"):
            check(self.root)


if __name__ == "__main__":
    unittest.main()
