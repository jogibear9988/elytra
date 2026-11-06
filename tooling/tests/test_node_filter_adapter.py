#!/usr/bin/env python3
"""Check definition-only adaptation, byte preservation and upstream drift rejection."""

import _support  # Shared build-helper import path for standalone test CLIs.

from pathlib import Path
from adapt_node_filter import ENTRY, REPLACEMENT, CONSTRUCTOR_EDIT, adapt

original = (
    (Path(__file__).resolve().parents[2] / "vendor/ladybird/Libraries/LibWeb/DOM/NodeFilter.cpp")
    .read_bytes()
    .decode("utf-8")
)
result = adapt(original)
assert (
    result.replace(REPLACEMENT, ENTRY).replace(CONSTRUCTOR_EDIT[1], CONSTRUCTOR_EDIT[0]) == original
)
assert "auto result = accept_node(realm, *traverser.filter(), node);" in result
for changed in (
    original.replace(ENTRY, "changed("),
    original + "\n" + ENTRY,
    original.replace(CONSTRUCTOR_EDIT[0], "changed-constructor"),
    original + "\n" + CONSTRUCTOR_EDIT[0],
):
    try:
        adapt(changed)
    except ValueError:
        pass
    else:
        raise AssertionError("Missing or duplicate entry was accepted")
print(
    "PASS NodeFilter adapter preserves all bytes outside declared definition/constructor seams and rejects entry drift"
)
