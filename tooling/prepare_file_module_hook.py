#!/usr/bin/env python3
"""Compose pinned file-script and native protocol fetch conditions without editing Ladybird's fetch implementation."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "1dd94814cadf7d93872b5b81f67bcb9b1b16d24ce0fd681328652fbb09dabe85"
SEAM = "            || is_file_to_file_font_load\n"
HOOK = "            || Firefly::FileModules::allows(*request)\n"
PREFIX = '#include "loading/FileModules.h"\n#include "loading/protocol/Dispatch.h"\n#include "native/protocol/OriginPolicy.h"\n'
PROTOCOL_SEAM = "    // 3. Switch on request’s current URL’s scheme and run the associated steps:\n"
PROTOCOL_HOOK = "    if (auto response = Firefly::ProtocolFetch::dispatch(fetch_params))\n        return *response;\n\n"

CORS_SEAM = (
    "        if (!Infrastructure::is_http_or_https_scheme(request->current_url().scheme())\n"
)
CORS_HOOK = "            && !Firefly::Protocols::is_cors_scheme(request->current_url().scheme())\n"
NETWORK_SEAM = "GC::Ref<PendingResponse> http_network_or_cache_fetch(JS::Realm& realm, Infrastructure::FetchParams const& fetch_params, IsAuthenticationFetch is_authentication_fetch, IsNewConnectionFetch is_new_connection_fetch)\n{\n"
NETWORK_HOOK = "    if (auto response = Firefly::ProtocolFetch::dispatch(fetch_params))\n        return *response;\n\n"


def adapt(source):
    if any(source.count(seam) != 1 for seam in (SEAM, PROTOCOL_SEAM, CORS_SEAM, NETWORK_SEAM)):
        raise ValueError("Review the file module fetch condition")
    return PREFIX + source.replace(SEAM, SEAM + HOOK).replace(
        PROTOCOL_SEAM, PROTOCOL_HOOK + PROTOCOL_SEAM
    ).replace(CORS_SEAM, CORS_SEAM + CORS_HOOK).replace(NETWORK_SEAM, NETWORK_SEAM + NETWORK_HOOK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/Libraries/LibWeb/Fetch/Fetching/Fetching.cpp"
    if hashlib.sha256(source.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review owned file module adapter before updating Ladybird")
    original = source.read_text()
    output = adapt(original)
    assert (
        output.removeprefix(PREFIX)
        .replace(SEAM + HOOK, SEAM)
        .replace(PROTOCOL_HOOK + PROTOCOL_SEAM, PROTOCOL_SEAM)
        .replace(CORS_SEAM + CORS_HOOK, CORS_SEAM)
        .replace(NETWORK_SEAM + NETWORK_HOOK, NETWORK_SEAM)
        == original
    )
    for invalid in [
        "",
        original + SEAM,
        original.replace(SEAM, ""),
        original + PROTOCOL_SEAM,
        original.replace(PROTOCOL_SEAM, ""),
        original + CORS_SEAM,
        original.replace(CORS_SEAM, ""),
        original + NETWORK_SEAM,
        original.replace(NETWORK_SEAM, ""),
    ]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/ambiguous fetch seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print(
        "PASS file module/protocol fetch seams, source fingerprint, exact byte preservation and drift rejection"
    )


if __name__ == "__main__":
    main()
