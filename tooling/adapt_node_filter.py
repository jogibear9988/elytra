#!/usr/bin/env python3
"""Adapt only the NodeFilter callback entry definition, preserving internal calls."""

import argparse
import json
from pathlib import Path

ENTRY = "JS::ThrowCompletionOr<DOM::NodeFilter::Result> accept_node("
REPLACEMENT = ENTRY.replace(" accept_node(", " firefly_original_accept_node(")

CONSTRUCTOR_EDIT = (
    "    : m_callback(callback)\n{",
    "    : m_callback(callback)\n{\n    Firefly::Callbacks::filter_created(*this, m_callback->callback->vm().current_realm());",
)


def adapt(text):
    if text.count(ENTRY) != 1:
        raise ValueError("Review upstream NodeFilter accept_node definition")
    before, after = CONSTRUCTOR_EDIT
    if text.count(before) != 1:
        raise ValueError("Review upstream NodeFilter constructor")
    return text.replace(ENTRY, REPLACEMENT).replace(before, after)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    text = source.read_bytes().decode("utf-8")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(
        (
            "// Generated NodeFilter entry adapter; do not edit.\n#line 1 "
            + json.dumps(str(source))
            + "\n"
            + adapt(text)
        ).encode("utf-8")
    )


if __name__ == "__main__":
    main()
