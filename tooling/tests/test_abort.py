#!/usr/bin/env python3
"""Compare cancellation globals and real Promise timers with pinned Node, without GUI."""

import _support  # Standalone regression imports.

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    count = 0
    with tempfile.TemporaryDirectory(prefix="mite-abort-") as directory:
        root = Path(directory)
        for source in sorted((ROOT / "mite/tests/abort").glob("*.mjs")):
            suffixes = [".mjs"]
            if source.stem in {
                "controller",
                "composition",
                "exceptions",
                "events",
                "unref",
                "listener-error",
            }:
                suffixes.append(".cjs")
            for suffix in suffixes:
                file = root / (source.stem + suffix)
                file.write_text(source.read_text())
                results = [
                    subprocess.run([binary, str(file)], capture_output=True, text=True, timeout=10)
                    for binary in [args.node, args.mite]
                ]
                status = 1 if source.stem == "listener-error" else 0
                assert all(result.returncode == status for result in results), (file.name, results)
                if source.stem == "differences":
                    assert results[0].stdout == "trusted true\nfalse cancelled:undefined\n", results
                    assert results[1].stdout == "trusted false\nfalse finished\n", results
                    print(
                        "INTENTIONAL DIFFERENCE: portable event trust metadata; synthetic events do not run private abort algorithms",
                        flush=True,
                    )
                elif source.stem == "emitter-once-policy":
                    assert (
                        results[0].stdout
                        == "synthetic ABORT_ERR false 0 0\nreentrant pending 1 1\npartial true 1 0\n"
                    ), results
                    assert (
                        results[1].stdout
                        == "synthetic payload false 0 0\nreentrant ABORT_ERR 0 0\npartial true 0 0\n"
                    ), results
                    print(
                        "INTENTIONAL DIFFERENCE: events.once ignores synthetic abort and cancels safely during registration",
                        flush=True,
                    )
                else:
                    assert results[0].stdout == results[1].stdout, (file.name, results)
                if status:
                    assert all(
                        "abort listener failure fixture" in result.stderr for result in results
                    ), results
                else:
                    assert all(not result.stderr for result in results), (file.name, results)
                assert "UNEXPECTED" not in results[1].stdout, results
                print("PASS abort:", file.name, flush=True)
                count += 1
    print(f"PASS {count} actual Node/Mite cancellation comparisons")


if __name__ == "__main__":
    main()
