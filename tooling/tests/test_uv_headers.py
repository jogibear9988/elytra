#!/usr/bin/env python3
"""Reject native library/socket imports that bypass the real WinSDK boundary."""

import _support
import contextlib
import io
import re
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
                with self.assertRaisesRegex(ValueError, re.escape(str(Path("firefly/Native.cpp")))):
                    check(root)
            source.write_text('#include "platform/UV.h"\n')
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)

    def test_openssl_requires_a_prior_owned_socket_import(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "mite").mkdir()
            (root / "firefly").mkdir()
            source = root / "mite/TLS.cpp"
            for text in (
                "#include <openssl/ssl.h>\n",
                '#include <openssl/ssl.h>\n#include "platform/WindowsSockets.h"\n',
                '# include "openssl/evp.h"\n',
                "#include <winsock2.h>\n",
                "#include <winsock.h>\n",
            ):
                source.write_text(text)
                with self.assertRaisesRegex(ValueError, re.escape(str(Path("mite/TLS.cpp")))):
                    check(root)
            for boundary in (
                "platform/WindowsSockets.h",
                "platform/UV.h",
                "../../../mite/runtime/platform/WindowsSockets.h",
            ):
                source.write_text(f'#include "{boundary}"\n#include <openssl/ssl.h>\n')
                with contextlib.redirect_stdout(io.StringIO()):
                    check(root)

    def test_sdk_only_fixture_is_not_an_openssl_exemption(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "mite").mkdir()
            source = root / "firefly/native/sandbox/windows/Probe.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("#include <winsock2.h>\n")
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
            source.write_text("#include <winsock2.h>\n#include <openssl/ssl.h>\n")
            with self.assertRaisesRegex(ValueError, "OpenSSL before socket boundary"):
                check(root)

    def test_actual_sdk_alias_and_scoped_macro_restoration(self):
        root = Path(__file__).resolve().parents[2]
        header = (root / "mite/runtime/platform/WindowsSockets.h").read_text()
        sdk = (root / "vendor/ladybird/AK/Windows.h").read_text()
        self.assertRegex(sdk, r"#\s*define timeval dummy_timeval")
        markers = [
            "#include <AK/Time.h>",
            '#pragma push_macro("timeval")',
            "#define timeval dummy_timeval",
            "#include <winsock2.h>",
            '#pragma pop_macro("timeval")',
        ]
        positions = [header.index(marker) for marker in markers]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("_TIMEVAL_DEFINED", header)


if __name__ == "__main__":
    unittest.main()
