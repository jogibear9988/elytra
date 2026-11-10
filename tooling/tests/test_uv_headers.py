#!/usr/bin/env python3
"""Reject new direct libuv includes that bypass the real WinSDK compatibility boundary."""

import _support
import contextlib
import io
from pathlib import Path
import tempfile
import unittest

from check_uv_headers import check


class UVHeaders(unittest.TestCase):
    def test_owned_sources(self):
        with contextlib.redirect_stdout(io.StringIO()):
            check()

    def test_refuses_both_direct_include_spellings(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "mite/runtime/platform").mkdir(parents=True)
            (root / "firefly").mkdir()
            (root / "mite/runtime/platform/UV.h").write_text("#include <uv.h>\n")
            source = root / "firefly/Native.cpp"
            for include in ("#include <uv.h>", '# include "uv.h"', "#include <uv/win.h>"):
                source.write_text(include + "\n")
                with self.assertRaisesRegex(ValueError, "firefly/Native.cpp"):
                    check(root)
            source.write_text('#include "platform/UV.h"\n')
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)


if __name__ == "__main__":
    unittest.main()
