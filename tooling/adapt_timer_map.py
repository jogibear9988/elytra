#!/usr/bin/env python3
"""Observe actual timer-map mutations with the correct explicit native owner, without macros."""

import argparse
import hashlib
import json
from pathlib import Path
import re

SOURCE_SHA256 = "b15f0b1c1674892420d526f9fcb80ebc2df219d7792c758891cd34af5ae34980"
HEADER_SHA256 = "bd1d086ac7bf499ccc6157361d329fa357e9942e4a5d92649c366da423d7681f"
CAST_OWNER = "global_scope"
COUNTS = {"m_timers": 9, "self.m_timers": 4, CAST_OWNER + ".m_timers": 1}
EXPRESSION = re.compile(
    re.escape(CAST_OWNER + ".m_timers") + r"\b|\bself\.m_timers\b|(?<![\w.])m_timers\b"
)


# The SDK's u64 internal timeout runs in chunks under one ID. Preserve the
# registration owner while it replaces its native Timer without a JS call frame.
REARM_BEFORE = """        Firefly::Timers::map_view(global_scope.m_timers, global_scope).remove(timer_key);
        if (remaining > 0) {
            global_scope.run_internal_timer(timer_key, remaining, completion_step);
            return;
        }
"""
REARM_AFTER = """        if (remaining > 0) {
            auto firefly_rearm = Firefly::Timers::map_view(global_scope.m_timers, global_scope).rearm(timer_key);
            global_scope.run_internal_timer(timer_key, remaining, completion_step);
            return;
        }
        Firefly::Timers::map_view(global_scope.m_timers, global_scope).remove(timer_key);
"""


def undo_rearm(source):
    if source.count(REARM_AFTER) != 1:
        raise ValueError("Missing/duplicate internal timer rearm composition")
    return source.replace(REARM_AFTER, REARM_BEFORE, 1)


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
    if result.count(REARM_BEFORE) != 1:
        raise ValueError("Review native internal timer chunk-rearm seam")
    result = result.replace(REARM_BEFORE, REARM_AFTER, 1)
    restored = undo_rearm(result)
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
