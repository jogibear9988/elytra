#!/usr/bin/env python3
"""Permission compositions preserve trust/activation/storage and ordinary browser semantics."""

import _support

import hashlib
import importlib.util
from pathlib import Path
import tempfile
from unittest.mock import patch
import unittest

import prepare_permission_hooks as cpp

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src"
spec = importlib.util.spec_from_file_location(
    "permission_composition", ROOT / "tooling/rust-permissions/notification.py"
)
rust = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rust)


class PermissionSources(unittest.TestCase):
    def test_cpp_exact_reversal_and_security_order(self):
        for name, (relative, expected) in cpp.PINS.items():
            original = (ROOT / "vendor/ladybird" / relative).read_text()
            self.assertEqual(hashlib.sha256(original.encode()).hexdigest(), expected)
            result = cpp.adapt(name, original)
            prefix, replacements = cpp.changes(name, original)
            restored = result.removeprefix(prefix)
            for before, after in reversed(replacements):
                self.assertEqual(restored.count(after), 1)
                restored = restored.replace(after, before, 1)
            self.assertEqual(restored, original)
            for before, _ in replacements:
                for invalid in (original.replace(before, "", 1), original + before):
                    with self.assertRaises(ValueError):
                        cpp.adapt(name, invalid)
            if name == "Permissions.cpp":
                self.assertLess(
                    result.index("!HTML::is_secure_context"),
                    result.index("RendererPermissions::query"),
                )
                self.assertLess(
                    result.index("RendererPermissions::query"),
                    result.index("get_permission_store_entry(feature, key)"),
                )
                self.assertIn(
                    "!Firefly::RendererPermissions::active() && settings->top_level_origin->is_opaque()",
                    result,
                )
            elif name == "WebContentPage.cpp":
                dispatch = result.index("Firefly::Permissions::dispatch_request")
                for gate in (
                    "if (!document)",
                    "permission_reaches_document(*permission_name",
                    "if (!is_trustworthy",
                    "!consume_transient_user_activation()",
                    "microphone_authorization() == MicrophoneAuthorization::Denied",
                ):
                    self.assertLess(
                        result.index(
                            gate, result.index("void WebContentPage::did_request_permission")
                        ),
                        dispatch,
                    )
                self.assertNotIn("Firefly::Permissions::dispatch_request", original)

    def test_rust_reversal_and_unchanged_native_activation(self):
        rust.verify(SOURCE)
        original = (SOURCE / "notifications_api/notification.rs").read_text()
        result = rust.compose(original)
        restored = result
        for before, after in reversed(rust.replacements()):
            self.assertEqual(restored.count(after), 1)
            restored = restored.replace(after, before, 1)
        self.assertEqual(restored, original)
        for statement in (
            "if !is_its_own_top_level_site {",
            "if !window.has_transient_activation() {",
            "window.consume_user_activation();",
        ):
            self.assertEqual(result.count(statement), original.count(statement))
        self.assertLess(
            result.index("let firefly_request_state"),
            result.index("if !window.has_transient_activation()"),
        )
        self.assertIn("request, Some(decision)", result)
        self.assertIn("decision.unwrap_or(PermissionState::Denied)", result)
        self.assertIn("255 => get_the_notifications_permission_state(&settings)", result)
        self.assertIn("get_the_notifications_permission_state(&request.settings)", result)
        # Generated task scheduling/captures and ordinary fallback remain exact.
        self.assertEqual(result.count("(request, permission_state as u8)"), 1)

    def test_rust_drift_and_double_application_fail(self):
        original = (SOURCE / "notifications_api/notification.rs").read_text()
        for before, _ in rust.replacements():
            for invalid in (original.replace(before, "", 1), original + before):
                with self.assertRaises(ValueError):
                    rust.compose(invalid)
        for invalid in (original + "\n", rust.compose(original)):
            with self.assertRaises(ValueError):
                rust.compose(invalid)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            for name in rust.PINS:
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((SOURCE / name).read_bytes())
            (source / "notifications_api/mod.rs").write_text("drift")
            with self.assertRaises(ValueError):
                rust.verify(source)

    def test_cpp_invalid_late_input_preserves_all_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "outputs"
            output.mkdir()
            for name, (relative, _) in cpp.PINS.items():
                source = root / "vendor/ladybird" / relative
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes((ROOT / "vendor/ladybird" / relative).read_bytes())
                (output / name).write_text("retained output")
            last = list(cpp.PINS.values())[-1][0]
            (root / "vendor/ladybird" / last).write_text("drift")
            with (
                patch.object(cpp, "ROOT", root),
                patch("sys.argv", ["prepare", "--output-dir", str(output)]),
            ):
                with self.assertRaises(SystemExit):
                    cpp.main()
            self.assertEqual({path.read_text() for path in output.iterdir()}, {"retained output"})

    def test_rust_test_abi_has_no_owned_session(self):
        original = "// retained upstream test stubs\n"
        result = rust.compose_stubs(original)
        self.assertEqual(result.removesuffix(rust.STUBS), original)
        self.assertIn("-> u8 {\n    255", result)
        self.assertIn("-> bool {\n    false", result)
        with self.assertRaises(ValueError):
            rust.compose_stubs(result)


if __name__ == "__main__":
    unittest.main()
