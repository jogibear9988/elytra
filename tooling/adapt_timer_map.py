#!/usr/bin/env python3
"""Observe actual timer-map mutations with the correct explicit native owner, without macros."""

import argparse
import hashlib
import json
from pathlib import Path
import re

SOURCE_SHA256 = "406a0ac318863b060b260b9ca89f41a942687d694978b0c600dbc333582dbdae"
HEADER_SHA256 = "2474e5002d89b0bfc1743cb587236f07d22803bc954ba0543903d17f94b56407"
CAST_OWNER = "as<WindowOrWorkerGlobalScopeMixin>(this_object)"
COUNTS = {"m_timers": 11, "self.m_timers": 4, CAST_OWNER + ".m_timers": 1}
EXPRESSION = re.compile(
    re.escape(CAST_OWNER + ".m_timers") + r"\b|\bself\.m_timers\b|(?<![\w.])m_timers\b"
)


def adapt(source):
    if "Firefly::Timers::map_view" in source:
        raise ValueError("Repeated timer-map composition")
    matches = list(EXPRESSION.finditer(source))
    actual = {expression: sum(match[0] == expression for match in matches) for expression in COUNTS}
    if actual != COUNTS or len(re.findall(r"\bm_timers\b", source)) != sum(COUNTS.values()):
        raise ValueError(f"Review actual timer-map owner expressions: {actual}")
    result = source
    journal = []
    for match in reversed(matches):
        expression = match[0]
        owner = "*this" if expression == "m_timers" else expression.removesuffix(".m_timers")
        replacement = f"Firefly::Timers::map_view({expression}, {owner})"
        journal.append((match.start(), expression, replacement))
        result = result[: match.start()] + replacement + result[match.end() :]
    restored = result
    for offset, before, after in reversed(journal):
        if restored[offset : offset + len(after)] != after:
            raise ValueError("Timer-map composition reversal offset changed")
        restored = restored[:offset] + before + restored[offset + len(after) :]
    if restored != source:
        raise ValueError("Timer-map composition changed unrelated source bytes")
    return result


def verify_source(source, header):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError("Review pinned timer source")
    if hashlib.sha256(header).hexdigest() != HEADER_SHA256:
        raise ValueError("Review pinned timer-map class ABI")
    return adapt(source.decode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = verify_source(args.source.read_bytes(), args.header.read_bytes())
    output = "// Generated owned timer-map owner adapter; do not edit.\n#line 1 "
    output += json.dumps(str(args.source.resolve()), ensure_ascii=False) + "\n" + result
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)


if __name__ == "__main__":
    main()
