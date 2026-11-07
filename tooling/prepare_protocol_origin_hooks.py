#!/usr/bin/env python3
"""Compose two exact origin-policy seams without editing pinned Ladybird sources."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '#include "native/protocol/OriginPolicy.h"\n'
ENTRIES = {
    "URL.cpp": (
        "Libraries/LibURL/URL.cpp",
        "3e233367b17349e9b5e305df0b44ff72c80167d529f392ab5c699195fee4c764",
        "    // NB: liburl_rust runs the remaining steps.\n",
        "    if (auto custom = Firefly::Protocols::custom_origin(*this); custom.has_value())\n        return custom.release_value();\n\n",
    ),
    "SecureContexts.cpp": (
        "Libraries/LibWebCommon/SecureContexts/AbstractOperations.cpp",
        "1e707372217a9a397c008ba6f3d8864a21295215a27145e90d2441692a7215e6",
        '    // 7. If origin’s scheme component is one which the user agent considers to be authenticated, return "Potentially Trustworthy".\n',
        "    if (Firefly::Protocols::is_secure_origin(origin))\n        return Trustworthiness::PotentiallyTrustworthy;\n\n",
    ),
}


def adapt(source, seam, hook):
    if source.count(seam) != 1:
        raise ValueError("Missing or ambiguous custom origin seam")
    result = PREFIX + source.replace(seam, hook + seam)
    assert result.removeprefix(PREFIX).replace(hook + seam, seam) == source
    return result


def prepare(output):
    planned = {}
    for name, (relative, digest, seam, hook) in ENTRIES.items():
        data = (ROOT / "vendor/ladybird" / relative).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(
                f"Review owned protocol origin seam before updating Ladybird: {relative}"
            )
        source = data.decode("utf-8")
        planned[name] = adapt(source, seam, hook)
        for invalid in ["", source + seam, source.replace(seam, "")]:
            try:
                adapt(invalid, seam, hook)
            except ValueError:
                pass
            else:
                raise AssertionError("Missing/duplicate origin seam accepted")
    if output is not None:
        output.mkdir(parents=True, exist_ok=True)
        for name, text in planned.items():
            path = output / name
            data = text.encode("utf-8")
            if not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)
    print("PASS pinned origin/secure-context seams, drift refusal and exact byte preservation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    prepare(args.output)


if __name__ == "__main__":
    main()
