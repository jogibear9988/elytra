#!/usr/bin/env python3
"""Pinned port provenance and fail-closed authenticated cache publication controls."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prefetch_native_assets as assets


class Response(io.BytesIO):
    status = 200
    headers = {
        "Content-Type": "application/json; charset=utf-8",
        "X-GitHub-Api-Version-Selected": "2022-11-28",
    }

    def geturl(self):
        return assets.URL


class Opener:
    def __init__(self, data):
        self.data = data
        self.requests = []

    def open(self, request, timeout):
        self.requests.append(request)
        return Response(self.data)


class PrefetchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "vendor/ladybird").mkdir(parents=True)
        for relative in ("vcpkg.json", "vendor/ladybird/vcpkg.json"):
            (self.root / relative).write_text(json.dumps({"builtin-baseline": assets.BASELINE}))
        self.objects = json.loads(
            (Path(__file__).parent / "fixtures/native-assets/pcre2-port.json").read_text()
        )
        self.patcher = patch.object(
            assets, "git_object", side_effect=lambda _, key: self.objects[key].encode()
        )
        self.patcher.start()
        self.addCleanup(self.patcher.stop)
        self.cache = self.root / "downloads"
        self.data = b'{"owned-test-payload":true}\n'
        self.original_digest = assets.ASSET_SHA512
        self.digest = patch.object(assets, "ASSET_SHA512", hashlib.sha512(self.data).hexdigest())
        self.digest.start()
        self.addCleanup(self.digest.stop)
        self.environment = {assets.TOKEN_ENV: "PRIVATE_TEST_TOKEN"}

    def fetch(self, opener=None, environment=None):
        return assets.prefetch(
            self.root,
            self.root / "vcpkg",
            self.cache,
            opener=opener or Opener(self.data),
            environment=self.environment if environment is None else environment,
        )

    def test_exact_pins_and_authenticated_atomic_cache_resume(self):
        assets.verify_pins(self.root, self.root / "vcpkg")
        opener = Opener(self.data)
        self.assertEqual(self.fetch(opener), "prefetched verified asset")
        self.assertEqual(opener.requests[0].full_url, assets.URL)
        self.assertEqual(opener.requests[0].get_header("Accept"), "*/*")
        self.assertEqual(
            opener.requests[0].get_header("Authorization"), "Bearer PRIVATE_TEST_TOKEN"
        )
        self.assertEqual((self.cache / assets.FILENAME).read_bytes(), self.data)
        self.assertEqual(self.fetch(environment={}), "verified cache")
        self.assertEqual(list(self.cache.iterdir()), [self.cache / assets.FILENAME])
        self.assertNotIn("PRIVATE_TEST_TOKEN", repr(list(self.cache.iterdir())))

    def test_actual_pinned_public_representation_requires_accept_header(self):
        payload = (
            Path(__file__).parent / "fixtures/native-assets/pcre2-sljit-gitlink.response"
        ).read_bytes()
        self.assertEqual(len(payload), 848)
        self.assertEqual(hashlib.sha512(payload).hexdigest(), self.original_digest)
        compact = json.dumps(json.loads(payload), separators=(",", ":")).encode()
        self.assertEqual(len(compact), 781)
        self.assertEqual(
            hashlib.sha512(compact).hexdigest(),
            "8b19209d2a8e0854af44a3845b98d9dcb20bb7a2be3937dd5e17ea37622afe3a671e48f44a5eaa0c406eb9801d1b3eefcf69c91b32c3bb2e6ac1a439d222d795",
        )
        with patch.object(assets, "ASSET_SHA512", self.original_digest):
            with self.assertRaises(assets.PrefetchError):
                self.fetch(Opener(compact))
        self.assertFalse(self.cache.exists())

        class NegotiatedOpener:
            def open(self, request, timeout):
                # Different representation is deliberately not canonicalized.
                return Response(payload if request.get_header("Accept") == "*/*" else b"{}")

        with patch.object(assets, "ASSET_SHA512", self.original_digest):
            self.assertEqual(self.fetch(NegotiatedOpener()), "prefetched verified asset")
        self.assertEqual((self.cache / assets.FILENAME).read_bytes(), payload)

    def test_diagnostics_never_echo_arbitrary_headers_or_token(self):
        class HostileResponse(Response):
            headers = {
                "Content-Type": "PRIVATE_TEST_TOKEN",
                "X-GitHub-Api-Version-Selected": "PRIVATE_TEST_TOKEN",
            }

        class HostileOpener:
            def open(self, request, timeout):
                return HostileResponse(self_data)

        self_data = self.data
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.fetch(HostileOpener())
        self.assertNotIn("PRIVATE_TEST_TOKEN", output.getvalue())
        self.assertIn('"contentType": "other"', output.getvalue())
        self.assertIn(hashlib.sha512(self.data).hexdigest(), output.getvalue())

    def test_baseline_port_manifest_and_version_drift_refused(self):
        for key in self.objects:
            original = self.objects[key]
            self.objects[key] = (
                original.replace(assets.PORT_TREE, "0" * 40)
                if "versions" in key
                else original + " "
            )
            with self.assertRaises(assets.PrefetchError):
                self.fetch()
            self.objects[key] = original
        (self.root / "vcpkg.json").write_text('{"builtin-baseline":"changed"}')
        with self.assertRaises(assets.PrefetchError):
            self.fetch()
        self.assertFalse(self.cache.exists())

    def test_malformed_pin_inputs_fail_before_network(self):
        key = assets.BASELINE + ":versions/p-/pcre2.json"
        for data in ("not-json", '{"versions":null}', '{"versions":[null]}'):
            self.objects[key] = data
            with self.assertRaises(assets.PrefetchError):
                self.fetch()
        self.assertFalse(self.cache.exists())

    def test_wrong_partial_and_oversize_payload_never_published(self):
        for data in (self.data[:-1], b"wrong", b"x" * (assets.MAX_BYTES + 1)):
            with self.assertRaises(assets.PrefetchError):
                self.fetch(Opener(data))
            self.assertFalse(self.cache.exists())
        self.cache.mkdir()
        (self.cache / assets.FILENAME).write_bytes(b"bad-existing-cache")
        with self.assertRaises(assets.PrefetchError):
            self.fetch()
        self.assertEqual((self.cache / assets.FILENAME).read_bytes(), b"bad-existing-cache")

    def test_missing_malformed_token_and_exception_logging(self):
        for token in ("", "bad\nheader"):
            with self.assertRaises(assets.PrefetchError) as caught:
                self.fetch(environment={assets.TOKEN_ENV: token})
            self.assertNotIn(token or "PRIVATE_TEST_TOKEN", str(caught.exception))

        class FailingOpener:
            def open(self, request, timeout):
                raise RuntimeError(request.get_header("Authorization"))

        with self.assertRaises(assets.PrefetchError) as caught:
            self.fetch(FailingOpener())
        self.assertNotIn("PRIVATE_TEST_TOKEN", str(caught.exception))
        self.assertTrue(caught.exception.__suppress_context__)

    def test_redirects_and_response_url_substitution_refused(self):
        request = assets.urllib.request.Request(assets.URL)
        for url in ("https://api.github.com/other", "https://evil.example/", assets.URL):
            with self.assertRaises(assets.PrefetchError):
                assets.NoRedirect().redirect_request(request, None, 302, "", {}, url)

        class WrongResponse(Response):
            def geturl(self):
                return "https://evil.example/"

        class RedirectedOpener:
            def open(self, request, timeout):
                return WrongResponse(b"payload")

        with self.assertRaises(assets.PrefetchError):
            self.fetch(RedirectedOpener())
        self.assertFalse(self.cache.exists())

    def test_deadline_failure_does_not_publish(self):
        with patch.object(assets.time, "monotonic", side_effect=[0, 0, assets.TIMEOUT + 1]):
            with self.assertRaises(assets.PrefetchError):
                self.fetch()
        self.assertFalse(self.cache.exists())

    def test_partial_read_and_failed_publication_clean_up(self):
        class InterruptedResponse(Response):
            def read(self, size):
                if self.tell():
                    raise OSError("PRIVATE_TEST_TOKEN")
                return super().read(2)

        class InterruptedOpener:
            def open(self, request, timeout):
                return InterruptedResponse(b"partial")

        with self.assertRaises(assets.PrefetchError):
            self.fetch(InterruptedOpener())
        self.assertFalse(self.cache.exists())
        with patch.object(assets.os, "replace", side_effect=OSError("publication failed")):
            with self.assertRaises(OSError):
                self.fetch()
        self.assertEqual(list(self.cache.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
