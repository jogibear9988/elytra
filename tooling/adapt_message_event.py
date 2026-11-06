#!/usr/bin/env python3
"""Adapt MessageEvent data tracing/init/getter without replacing messaging."""

import argparse
import json
from pathlib import Path
from adapt_custom_event import BINDING_EDIT, CONSTRUCTOR_EDIT

NATIVE_EDITS = (
    (
        "    visitor.visit(m_data);",
        "    visitor.visit(m_data);\n    Firefly::EventValues::visit(*this, visitor);",
    ),
    (
        "    m_data = data;",
        "    m_data = data;\n    Firefly::EventValues::changed(*this, vm().current_realm());",
    ),
)


def adapt(text, kind):
    if kind == "native":
        edits = NATIVE_EDITS
    else:
        before, after = CONSTRUCTOR_EDIT
        if text.count(before) != 1:
            raise ValueError("Review generated MessageEvent constructor")
        text = text.replace(before, after)
        marker = "JS_DEFINE_NATIVE_FUNCTION(MessageEventPrototype::data_getter)\n"
        if text.count(marker) != 1:
            raise ValueError("Review generated MessageEvent data getter")
        start = text.index(marker)
        end = text.index("\n}\n", start) + 3
        prefix, text, suffix = text[:start], text[start:end], text[end:]
        edits = (BINDING_EDIT,)
    for before, after in edits:
        if text.count(before) != 1:
            raise ValueError("Review MessageEvent seam: " + before)
        text = text.replace(before, after)
    return text if kind == "native" else prefix + text + suffix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--kind", required=True, choices=["native", "binding"])
    args = parser.parse_args()
    source = args.source.resolve()
    original = source.read_bytes().decode("utf-8")
    result = (
        "// Generated MessageEvent data adapter; do not edit.\n#line 1 "
        + json.dumps(str(source), ensure_ascii=False)
        + "\n"
        + adapt(original, args.kind)
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result.encode("utf-8"))


if __name__ == "__main__":
    main()
