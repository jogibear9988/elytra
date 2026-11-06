#!/usr/bin/env python3
"""Adapt explicit CustomEvent value-storage/tracing/getter seams outside upstream."""

import argparse
import json
from pathlib import Path

NATIVE_EDITS = (
    (
        "    , m_detail(event_init.detail)\n{",
        "    , m_detail(event_init.detail)\n{\n    Firefly::EventValues::changed(*this, nullptr);",
    ),
    (
        "    visitor.visit(m_detail);",
        "    visitor.visit(m_detail);\n    Firefly::EventValues::visit(*this, visitor);",
    ),
    (
        "    m_detail = detail;",
        "    m_detail = detail;\n    Firefly::EventValues::changed(*this, vm().current_realm());",
    ),
)
CONSTRUCTOR_EDIT = (
    "    auto wrapper = wrap(host_defined_wrapper_world(realm), realm, impl);",
    "    Firefly::EventValues::changed(*impl, &realm);\n    auto wrapper = wrap(host_defined_wrapper_world(realm), realm, impl);",
)
BINDING_EDIT = ("    return R;", "    return Firefly::EventValues::read(realm, *idl_object, R);")


def adapt(text, kind):
    if kind == "native":
        edits = NATIVE_EDITS
    else:
        before, after = CONSTRUCTOR_EDIT
        if text.count(before) != 1:
            raise ValueError("Review generated event constructor")
        text = text.replace(before, after)
        for entry in ["detail_getter", "init_custom_event"]:
            if text.count("JS_DEFINE_NATIVE_FUNCTION(CustomEventPrototype::" + entry + ")\n") != 1:
                raise ValueError("Review generated CustomEvent entry: " + entry)
        start = text.index("JS_DEFINE_NATIVE_FUNCTION(CustomEventPrototype::detail_getter)\n")
        end = text.index(
            "JS_DEFINE_NATIVE_FUNCTION(CustomEventPrototype::init_custom_event)\n", start
        )
        prefix, text, suffix = text[:start], text[start:end], text[end:]
        edits = (BINDING_EDIT,)
    for before, after in edits:
        if text.count(before) != 1:
            raise ValueError("Review CustomEvent seam: " + before)
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
        "// Generated CustomEvent value adapter; do not edit.\n#line 1 "
        + json.dumps(str(source), ensure_ascii=False)
        + "\n"
        + adapt(original, args.kind)
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result.encode("utf-8"))


if __name__ == "__main__":
    main()
