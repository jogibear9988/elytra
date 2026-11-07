#!/usr/bin/env python3
"""Exercise trusted byte endpoint delivery in real headless WebContent processes."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import re
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    probe = args.probe.resolve()
    if not probe.is_file():
        parser.error("Build the byte renderer probe first")
    page = (ROOT / "firefly/tests/ipc/byte-channel.html").as_uri()
    pids = set()
    for options in [[], ["--upstream-control"], ["--invalid-cookie"]]:
        result = run(probe, *options, page)
        match = re.search(r"^WebContent PID: (\d+)$", result.stdout, re.MULTILINE)
        assert result.returncode == 0 and match, result
        pid = int(match[1])
        assert pid > 0 and pid != result.pid, result
        pids.add(pid)
        expected = (
            "BYTE_HANDOFF_DENIED_DOCUMENT_ALIVE" if options else "BYTE_HANDOFF_OK bytes=65536"
        )
        assert expected in result.stdout, result
        if options:
            assert "Firefly byte bootstrap unavailable/invalid" in result.stderr, result
        else:
            assert "BYTE_DUPLICATE_DENIED" in result.stdout, result
        assert "Renderer crashed" not in result.stderr, result
    assert len(pids) == 3, pids
    print(
        "PASS actual WebContent handoff/PID/cookie and 65,536 arbitrary bytes; document stays live"
    )
    print("PASS default-deny upstream helper and zero cookie close endpoint; control/DOM survive")


if __name__ == "__main__":
    main()
