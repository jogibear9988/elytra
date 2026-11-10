#!/usr/bin/env python3
"""Compose embedded binary-path/event-loop selection into the exact upstream Hub."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "Services/Hub/Application.cpp"
DIGEST = "1d7a53fa6a23faa3bb841304daabad28496b1f0f01c1d62b6d05761d1bab40cf"
PREFIX = '#include "native/qt/hub/Lifecycle.h"\n'
CONSTRUCTOR = "Application::Application(ApplicationHost* host)\n    : m_host(host)\n"
OWNED_CONSTRUCTOR = (
    "Application::Application(ApplicationHost* host)\n"
    "    : WebView::Application(Firefly::HubLifecycle::embedded_binary_directory())\n"
    "    , m_host(host)\n"
)
LOOP = "    auto& event_loop = WebView::Application::create_platform_event_loop();\n"
OWNED_LOOP = (
    "    auto* embedded_loop = Firefly::HubLifecycle::embedded_event_loop();\n"
    "    auto& event_loop = embedded_loop ? *embedded_loop\n"
    "                                     : WebView::Application::create_platform_event_loop();\n"
)
SEAMS = ((CONSTRUCTOR, OWNED_CONSTRUCTOR), (LOOP, OWNED_LOOP))


def revert(source):
    """Remove only the declared ownership choices; preserve host notifications."""
    if not source.startswith(PREFIX) or source.count(PREFIX) != 1:
        raise ValueError("Review embedded Hub prefix")
    result = source.removeprefix(PREFIX)
    for before, after in SEAMS:
        if result.count(after) != 1:
            raise ValueError("Review embedded Hub reversal")
        result = result.replace(after, before)
    return result


def adapt(source):
    if PREFIX in source:
        raise ValueError("Repeated embedded Hub composition")
    result = source
    for before, after in SEAMS:
        if result.count(before) != 1 or after in result:
            raise ValueError("Review embedded Hub lifecycle seam")
        result = result.replace(before, after)
    result = PREFIX + result
    if revert(result) != source:
        raise ValueError("Embedded Hub changed unrelated upstream bytes")
    return result


def prepare(source, destination):
    """Validate the entire source and both seams before writing generated output."""
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != DIGEST:
        raise ValueError("Review embedded Hub lifecycle source pin")
    original = data.decode("utf-8")
    output = adapt(original)
    for before, _ in SEAMS:
        for invalid in (original.replace(before, ""), original + before):
            try:
                adapt(invalid)
            except ValueError:
                pass
            else:
                raise AssertionError("Missing/duplicate Hub lifecycle seam accepted")
    try:
        adapt(output)
    except ValueError:
        pass
    else:
        raise AssertionError("Repeated Hub lifecycle composition accepted")
    if not destination.exists() or destination.read_bytes() != output.encode():
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(output.encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(ROOT / "vendor/ladybird" / SOURCE, args.output)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    print("PASS embedded Hub lifecycle fingerprint, exact reversal and seam refusal")


if __name__ == "__main__":
    main()
