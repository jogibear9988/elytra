#!/usr/bin/env python3
"""Source-level checks for the owned Cocoa AX adapter; never starts Qt/AppKit."""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "view_accessibility_hook", ROOT / "tooling/prepare_view_accessibility_hook.py"
)
adapter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(adapter)


class AccessibilityAdapterTests(unittest.TestCase):
    def test_preserves_remote_view_tree_and_delta_bytes(self):
        source = ROOT / "vendor/ladybird/UI/Qt"
        original = (source / "WebContentViewAccessibility.mm").read_text()
        for name in adapter.EXPECTED:
            adapter.validate(name, (source / name).read_bytes())
        generated = adapter.adapt(original)
        # Independently remove only our six known insertions. Every native tree,
        # per-widget manager, action, zoom and removed-node path must be identical.
        include = "#include <QGuiApplication>\n"
        self.assertEqual(generated.count(include), 1)
        restored = generated.replace(include, "", 1)
        for action, count in (("return;", 4), ("return nil;", 1)):
            inserted = (
                '\n    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))\n'
                f"        {action}\n"
            )
            self.assertEqual(restored.count(inserted), count)
            restored = restored.replace(inserted, "")
        self.assertEqual(restored, original)
        self.assertIn("for (auto id : *removed_ids)", generated)
        self.assertIn("manager:view->m_accessibility_manager.ptr()", generated)
        self.assertIn("_webContentView->perform_accessibility_action", generated)

    def test_refuses_missing_duplicate_repeated_and_source_header_drift(self):
        source = ROOT / "vendor/ladybird/UI/Qt"
        original = (source / "WebContentViewAccessibility.mm").read_text()
        adapter.verify_negative_controls(original)
        for name in adapter.EXPECTED:
            with self.assertRaises(ValueError):
                adapter.validate(name, (source / name).read_bytes() + b"\n")


if __name__ == "__main__":
    unittest.main()
