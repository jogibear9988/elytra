#!/usr/bin/env python3
"""Exact reversal/default-off and bounded no-payload diagnostic controls."""

import _support
import argparse
import hashlib
from pathlib import Path
import tempfile

from code_oss import scanner_diagnostics as scanner
from code_oss.overlay import prepare

ROOT = Path(__file__).resolve().parents[2]


def rejects(callback):
    try:
        callback()
    except (ValueError, TypeError):
        return
    raise AssertionError("Expected scanner drift refusal")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    original = (args.source / scanner.SOURCE).read_bytes()
    patched = scanner.adapt_scanner(original)
    assert scanner.revert_scanner(patched) == original
    assert patched.count(b"await ") == original.count(b"await ")
    rejects(lambda: scanner.adapt_scanner(original + b"\n"))
    rejects(lambda: scanner.adapt_scanner(patched))
    rejects(lambda: scanner.revert_scanner(patched + b"\n"))
    rejects(lambda: scanner.revert_scanner(patched.replace(b"scanEnd('stat'", b"scanEnd('other'")))
    with tempfile.TemporaryDirectory(prefix="elytra-scanner-source-") as temporary:
        temporary = Path(temporary)
        default = prepare(args.source, temporary / "default")
        assert not any(
            item["path"] in (scanner.SOURCE, scanner.HELPER_TARGET) for item in default["files"]
        )
        rejects(
            lambda: prepare(args.source, temporary / "retired", extension_scan_diagnostics=True)
        )
        assert not (ROOT / scanner.HELPER_SOURCE).exists()
    assert (args.source / scanner.SOURCE).read_bytes() == original
    print(
        "PASS scanner exact pin/reversal/drift, retired overlay/helper absent, historical awaits unchanged"
    )


if __name__ == "__main__":
    main()
