#!/usr/bin/env python3
"""Prove pinned owner expressions, byte preservation and timer-map source drift rejection."""

import _support
from pathlib import Path
from adapt_timer_map import (
    COUNTS,
    CAST_OWNER,
    REARM_BEFORE,
    REARM_AFTER,
    adapt,
    undo_rearm,
    verify_source,
)

root = Path(__file__).resolve().parents[2]
directory = root / "vendor/ladybird/Libraries/LibWeb/HTML"
source = (directory / "WindowOrWorkerGlobalScope.cpp").read_bytes()
header = (directory / "WindowOrWorkerGlobalScope.h").read_bytes()
original = source.decode("utf-8")
result = verify_source(source, header)
restored = undo_rearm(result)
for expression, count in COUNTS.items():
    owner = "*this" if expression == "m_timers" else expression.removesuffix(".m_timers")
    replacement = f"Firefly::Timers::map_view({expression}, {owner})"
    assert restored.count(replacement) == count
    restored = restored.replace(replacement, expression)
assert restored == original, "Adapter changed bytes outside the actual map expressions"
assert result.count("Firefly::Timers::map_view(") == sum(COUNTS.values()) + 1
assert REARM_AFTER in result
assert "self.Firefly::" not in result
assert ").Firefly::" not in result
for expression in COUNTS:
    for invalid in [
        original.replace(expression, "changed_timer_map", 1),
        original + "\n" + expression,
    ]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/duplicate timer owner was accepted")
for invalid in [
    result,
    original.replace("self.m_timers", "other.m_timers", 1),
    original.replace(CAST_OWNER + ".m_timers", "unknown_owner.m_timers", 1),
]:
    try:
        adapt(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError("Repeated/unknown timer owner was accepted")
for changed in [
    original.replace("        if (remaining > 0) {", "        if (remaining >= 0) {", 1),
    original.replace(
        "global_scope.run_internal_timer(timer_key, remaining, completion_step);", "", 1
    ),
]:
    try:
        adapt(changed)
    except ValueError:
        pass
    else:
        raise AssertionError("Changed internal timer rearm was accepted")
for changed_source, changed_header in [(source + b"\n", header), (source, header + b"\n")]:
    try:
        verify_source(changed_source, changed_header)
    except ValueError:
        pass
    else:
        raise AssertionError("Pinned source/class drift was accepted")
print("PASS timer-map exact14 owners, rooted internal rearm, byte preservation and drift rejection")
