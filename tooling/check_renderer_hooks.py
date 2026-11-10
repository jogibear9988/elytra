#!/usr/bin/env python3
"""Reject an upstream update that changes the narrow renderer entry adapters."""

from pathlib import Path
import re
from adapt_timer_map import verify_source as verify_timer_source

ROOT = Path(__file__).resolve().parents[1]
HOOKS = {
    "Services/WebContent/main.cpp": ["ladybird_main", "initialize_agent"],
    "Services/WebContent/WebDriverBiDi.cpp": [
        "set_webdriver_bidi_session_state",
        "run_webdriver_bidi_command",
    ],
    "Libraries/LibWeb/WebIDL/AbstractOperations.cpp": ["invoke_callback"],
    "Libraries/LibWeb/HTML/WindowOrWorkerGlobalScope.cpp": ["m_timers"],
    "Libraries/LibWeb/DOM/EventTarget.cpp": [
        "invoke_event_listener",
        "report_exception_for_callback",
        "invoke_event_handler",
        "event_listener_list_changed",
        "add_abort_algorithm",
    ],
    "Libraries/LibWeb/DOM/MutationObserver.cpp": ["m_node_list"],
}


EXPECTED_COUNTS = {
    "invoke_callback": 4,
    "m_timers": 16,
    "m_node_list": 3,
    "event_listener_list_changed": 2,
    "invoke_event_handler": 2,
}


def verify_text(text, names):
    failures = []
    for name in names:
        if len(re.findall(r"\b" + re.escape(name) + r"\b", text)) != EXPECTED_COUNTS.get(name, 1):
            failures.append(f"Review renderer adapter for changed entry point: {name}")
    if failures:
        raise ValueError("; ".join(failures))


def main():
    failures = []
    for relative, names in HOOKS.items():
        try:
            verify_text((ROOT / "vendor/ladybird" / relative).read_text(), names)
        except (ValueError, OSError) as error:
            failures.append(f"{relative}: {error}")
    timer_directory = ROOT / "vendor/ladybird/Libraries/LibWeb/HTML"
    try:
        verify_timer_source(
            (timer_directory / "WindowOrWorkerGlobalScope.cpp").read_bytes(),
            (timer_directory / "WindowOrWorkerGlobalScope.h").read_bytes(),
        )
    except (ValueError, OSError) as error:
        failures.append(f"Timer owner composition: {error}")
    if failures:
        raise SystemExit("\n".join(failures))
    print("Narrow renderer entry adapters verified")


if __name__ == "__main__":
    main()
