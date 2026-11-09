#!/usr/bin/env python3
"""Contain the pinned WebSocket path which still bypasses Fetch interception."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "fd05dffb7fc7ee5c1763bfc8ab2d306f623bbb43a2e252ce1677f3cf9000a5ec"
PREFIX = '#include "loading/web-request/Gate.h"\n'
SEAM = "ErrorOr<void> WebSocket::establish_web_socket_connection(URL::URL const& url_record, Vector<Utf16String> const& protocols, HTML::EnvironmentSettingsObject& client)\n{\n"
HOOK = """    // The upstream opening handshake does not yet use Fetch. Refuse it
    // while session interception is active rather than bypassing application policy.
    auto firefly_policy = Firefly::WebRequestGate::policy();
    if (firefly_policy.before || firefly_policy.headers)
        return Error::from_string_literal("WebSocket interception by webRequest is unsupported");
\n"""


def adapt(source):
    if source.count(SEAM) != 1 or PREFIX in source or HOOK in source:
        raise ValueError("Review native WebSocket containment seam")
    return PREFIX + source.replace(SEAM, SEAM + HOOK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/Libraries/LibWeb/WebSockets/WebSocket.cpp"
    if hashlib.sha256(source.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review WebSocket containment before updating Ladybird")
    original = source.read_text()
    output = adapt(original)
    assert output.removeprefix(PREFIX).replace(SEAM + HOOK, SEAM) == original
    for invalid in ("", original.replace(SEAM, ""), original + SEAM, output):
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/duplicate WebSocket seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print("PASS native WebSocket containment fingerprint, byte preservation and drift controls")


if __name__ == "__main__":
    main()
