#!/usr/bin/env python3
"""Reject ErrorEvent native/bindgen seam drift and preserve unrelated bytes."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from adapt_error_event import NATIVE_EDITS, adapt
from adapt_custom_event import BINDING_EDIT, CONSTRUCTOR_EDIT
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--native", required=True, type=Path)
parser.add_argument("--binding", required=True, type=Path)
args = parser.parse_args()
for kind, path, edits in [
    ("native", args.native, NATIVE_EDITS),
    ("binding", args.binding, (CONSTRUCTOR_EDIT, BINDING_EDIT)),
]:
    original = path.read_bytes().decode("utf-8")
    modified = adapt(original, kind)
    restored = modified
    for before, after in reversed(edits):
        assert restored.count(after) == 1, (kind, after)
        restored = restored.replace(after, before)
        try:
            adapt(original.replace(before, "changed-error-seam", 1), kind)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing error seam accepted")
    assert restored == original, "Unrelated error source changed"
    if kind == "native":
        for before, after in edits:
            try:
                adapt(original + "\n" + before, kind)
            except ValueError:
                pass
            else:
                raise AssertionError("Duplicate error seam accepted")
    else:
        marker = "JS_DEFINE_NATIVE_FUNCTION(ErrorEventPrototype::error_getter)\n"
        try:
            adapt(original + "\n" + marker, kind)
        except ValueError:
            pass
        else:
            raise AssertionError("Duplicate error getter accepted")
        extra = "\nJS::Value unrelated() {\n    return R;\n}\n"
        assert adapt(original + extra, kind).endswith(extra)
print("PASS ErrorEvent native/generated seams reject drift and preserve other source bytes")
