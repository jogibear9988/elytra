#!/usr/bin/env python3
"""Compose native autoplay enforcement into the exact pinned media implementation."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "Libraries/LibWeb/HTML/HTMLMediaElement.cpp"
DIGEST = "1f04477f79cc7e64b687720c97ce5122d8402890af8c2888a09837dbf2f8d103"
PREFIX = '#include "autoplay/Media.h"\n'
SEAMS = [
    (
        "HTMLMediaElement::~HTMLMediaElement() = default;",
        "HTMLMediaElement::~HTMLMediaElement()\n{\n    Firefly::Autoplay::destroyed(*this);\n}",
    ),
    *[
        (signature + "\n{\n", signature + "\n{\n    Firefly::Autoplay::" + hook + "(*this);\n")
        for signature, hook in [
            ("void HTMLMediaElement::initialize_element()", "initialize"),
            ("void HTMLMediaElement::play(GC::Ref<WebIDL::Promise> promise)", "request_play"),
            ("WebIDL::ExceptionOr<void> HTMLMediaElement::load()", "request_load"),
            ("void HTMLMediaElement::volume_or_muted_attribute_changed()", "request_audible"),
            ("void HTMLMediaElement::play_from_user_interaction()", "native_play"),
        ]
    ],
    (
        "void HTMLMediaElement::adopted_from(DOM::Document& old_document)\n{\n    Base::adopted_from(old_document);\n",
        "void HTMLMediaElement::adopted_from(DOM::Document& old_document)\n{\n    Base::adopted_from(old_document);\n    Firefly::Autoplay::adopted(*this, old_document);\n",
    ),
    (
        "bool HTMLMediaElement::is_allowed_to_play() const\n{\n",
        "bool HTMLMediaElement::is_allowed_to_play() const\n{\n    if (auto decision = Firefly::Autoplay::allowed(*this); decision.has_value())\n        return *decision;\n",
    ),
]


def reverse(output):
    if not output.startswith(PREFIX):
        raise ValueError("Missing native autoplay composition prefix")
    result = output[len(PREFIX) :]
    for original, replacement in reversed(SEAMS):
        if result.count(replacement) != 1:
            raise ValueError("Review composed native autoplay seam")
        result = result.replace(replacement, original)
    return result


def adapt(original):
    if PREFIX in original or any(original.count(needle) != 1 for needle, _ in SEAMS):
        raise ValueError("Review missing/duplicate/repeated native autoplay seam")
    if hashlib.sha256(original.encode()).hexdigest() != DIGEST:
        raise ValueError("Review native autoplay source pin after Ladybird update")
    result = original
    for needle, replacement in SEAMS:
        result = result.replace(needle, replacement)
    result = PREFIX + result
    if reverse(result) != original:
        raise ValueError("Native autoplay composition changed unrelated source bytes")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "vendor/ladybird" / SOURCE)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = adapt(args.source.read_bytes().decode()).encode()
    except (ValueError, UnicodeError) as error:
        raise SystemExit(str(error)) from error
    args.output_dir.mkdir(parents=True, exist_ok=True)
    target = args.output_dir / "HTMLMediaElement.cpp"
    if not target.exists() or target.read_bytes() != output:
        target.write_bytes(output)
    print("PASS native autoplay media source pin and exact composition reversal")


if __name__ == "__main__":
    main()
