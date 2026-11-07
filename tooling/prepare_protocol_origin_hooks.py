#!/usr/bin/env python3
"""Compose origin, fetch-scheme and helper-launch seams without editing pinned Ladybird sources."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '#include "native/protocol/OriginPolicy.h"\n'
ENTRIES = {
    "FetchURL.cpp": (
        "Libraries/LibWebCommon/Fetch/Infrastructure/URL.cpp",
        "4b80c8ec28036075fb142b475d0459df3b80a0181779a00ccb74f866e4524d33",
        '    // A fetch scheme is "about", "blob", "data", "file", or an HTTP(S) scheme.\n',
        "    if (Firefly::Protocols::is_standard_scheme(scheme))\n        return true;\n\n",
    ),
    "HelperProcess.cpp": (
        "Libraries/LibWebView/HelperProcess.cpp",
        "b534514372909dec44a6a31738825e0d92aabea923add1fa2067647f82a1d888",
        '    auto client = TRY(launch_server_process<WebView::WebContentClient>("WebContent"sv, move(arguments), is_private, initial_page_id, root_navigable_id));\n',
        '    if (auto configuration = Firefly::Protocols::configuration_for_helper_launch(); configuration.has_value())\n        arguments.append(ByteString::formatted("--firefly-origin-policy={}", *configuration));\n\n',
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
    print(
        "PASS pinned origin/trust/fetch-scheme/helper-launch seams, drift refusal and exact byte preservation"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    prepare(args.output)


if __name__ == "__main__":
    main()
