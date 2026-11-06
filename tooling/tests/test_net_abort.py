#!/usr/bin/env python3
"""Compare real TCP cancellation, signal ownership and close behavior with Node."""

import _support  # Shared build-helper path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    root = Path(__file__).resolve().parents[2] / "mite/tests/net-abort"
    expected = {
        "pre-connecting": "pre connect 0 1 true\n",
        "connected": "connected true true 1 true\n",
        "lookup": "lookup true false 0 1 true 1 true\n",
        "server-pre": "server pre true 0 0 false true\n",
        "server-wait": "server wait true 0 false true\n",
        "server-reuse": "reuse true 2\n",
        "propagation": "propagation true true\n",
        "validation": "validation 15\n",
        "server-immediate": "immediate server 0 false\n",
        "socket-constructor": "constructor true true\n",
    }
    for file in sorted(root.glob("*.mjs")):
        results = [
            subprocess.run([exe, str(file)], capture_output=True, text=True, timeout=20)
            for exe in [args.node, args.mite]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), (
            file.name,
            results,
        )
        if file.stem == "server-failure":
            # A failed bind must not let its stale signal close a later listener.
            assert results[0].stdout == "failure release false\n", results
            assert results[1].stdout == "failure release true\n", results
        elif file.stem == "synthetic":
            assert results[0].stdout == "synthetic socket true true\n", results
            assert results[1].stdout == "synthetic socket false true\n", results
        elif file.stem == "pre-aborted":
            # Node resets an unconnected Socket for later reuse after its abort;
            # Mite explicitly does not support reconnecting destroyed Sockets.
            assert results[0].stdout == "pre returned,error,close false false\n", results
            assert results[1].stdout == "pre returned,error,close true false\n", results
        else:
            assert results[0].stdout == results[1].stdout == expected[file.stem], (
                file.name,
                results,
            )
        print("PASS actual TCP cancellation:", file.name, flush=True)


if __name__ == "__main__":
    main()
