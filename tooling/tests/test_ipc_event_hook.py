#!/usr/bin/env python3
"""Pinned native-navigation observations preserve every upstream byte on reversal."""

import _support
import hashlib
from pathlib import Path
import re
from prepare_ipc_event_hook import ROOT, EXPECTED, PREFIX, PHASES, adapt, revert
from prepare_permission_hooks import adapt as permission_adapt


def rejected(callback):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError("Navigation hook accepted drift")


def main():
    path = ROOT / "vendor/ladybird/Libraries/LibWebView/WebContentPage.cpp"
    raw = path.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == EXPECTED
    original = raw.decode()
    generated = adapt(original)
    assert revert(generated) == original
    # The independent permission composition still accepts the same translation
    # unit; diagnostics must neither replace nor drop either owned boundary.
    composed = permission_adapt("WebContentPage.cpp", generated)
    assert 'Firefly::diagnose_navigation_phase("after-native-document");' in composed
    calls = re.findall(r'Firefly::diagnose_navigation_phase\("([a-z-]+)"(.*?)\);', generated)
    labels = {
        "before-native-navigation",
        "after-native-navigation",
        "population-entry",
        "handoff-callback",
        "handoff-request",
        "handoff-submitted",
        "adopt-request",
        "adopt-callback",
        "adopt-submitted",
        "before-native-document",
        "after-native-document",
        "before-populate",
        "after-populate",
    }
    assert len(calls) == len(labels) and {label for label, _ in calls} == labels
    assert all(
        argument == (", succeeded ? 1 : 0" if label == "handoff-callback" else "")
        for label, argument in calls
    )
    rejected(lambda: adapt(original + "\n"))
    rejected(lambda: adapt(generated))
    rejected(lambda: revert(PREFIX + generated))
    rejected(lambda: revert(generated + "\n"))
    for seam, replacement in PHASES:
        rejected(lambda: adapt(original.replace(seam, "")))
        rejected(lambda: adapt(original + seam))
        rejected(lambda: revert(generated.replace(replacement, "")))
        rejected(lambda: revert(generated + replacement))
    assert path.read_bytes() == raw
    print(
        "PASS native navigation diagnostics: exact pin/reversal, scalar whitelist, drift/duplicate refusal and permission composition"
    )


if __name__ == "__main__":
    main()
