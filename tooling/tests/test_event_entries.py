#!/usr/bin/env python3
"""Verify declared native adapter seams and preserve every other upstream byte."""

import _support  # Shared build-helper import path for standalone test CLIs.

import difflib
from pathlib import Path
from adapt_event_entries import ENTRIES, STORAGE_EDITS, adapt

source = (
    Path(__file__).resolve().parents[2] / "vendor/ladybird/Libraries/LibWeb/DOM/EventTarget.cpp"
)
original = source.read_bytes().decode("utf-8")
modified = adapt(original)
restored = modified
for before, after, count in reversed(STORAGE_EDITS):
    assert restored.count(after) == count, (before, count)
    restored = restored.replace(after, before)
for name, result_type in ENTRIES.items():
    restored = restored.replace(
        result_type + " firefly_original_" + name + "(", result_type + " " + name + "("
    )
assert restored == original, "Adapter changed bytes outside declared native seams"
for before, after, count in STORAGE_EDITS:
    for invalid in [original.replace(before, "changed-storage-seam", 1), original + "\n" + before]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Changed/duplicate storage seam accepted")
entry_only = modified
for before, after, count in reversed(STORAGE_EDITS):
    entry_only = entry_only.replace(after, before)
changes = [
    line
    for line in difflib.ndiff(original.splitlines(), entry_only.splitlines())
    if line.startswith(("- ", "+ "))
]
assert len(changes) == 2 * len(ENTRIES), changes
for name, result_type in ENTRIES.items():
    before = [
        line[2:] for line in changes if line.startswith("- " + result_type + " " + name + "(")
    ]
    after = [
        line[2:]
        for line in changes
        if line.startswith("+ " + result_type + " firefly_original_" + name + "(")
    ]
    assert (
        len(before) == len(after) == 1
        and before[0].replace(name, "firefly_original_" + name, 1) == after[0]
    ), changes
    for invalid in [
        original.replace(result_type + " " + name + "(", result_type + " changed_" + name + "(", 1),
        original + "\n" + result_type + " " + name + "(\n",
    ]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Changed/duplicate definition accepted")
assert "Bindings::invoke_event_handler(*callback, event, special_error_event_handling)" in modified
print(
    "PASS generated adapter preserves bytes outside declared entry/storage/trace/scope seams and rejects drift"
)
