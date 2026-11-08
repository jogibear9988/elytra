#!/usr/bin/env python3
"""Compare genuine fs.watch native events and disposal with Node 24, without GUI apps."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


def referenced(binary, mode):
    with tempfile.TemporaryDirectory(prefix="mite-event-ref-") as directory:
        work = Path(directory)
        child = subprocess.Popen(
            [binary, str(ROOT / "mite/tests/event-watcher/referenced.mjs"), mode],
            cwd=work,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            deadline = time.monotonic() + 2
            while not (work / "reference-ready").exists() and time.monotonic() < deadline:
                assert child.poll() is None, "Referenced watcher exited before setup"
                time.sleep(0.01)
            assert (work / "reference-ready").exists(), "Watcher setup timed out"
            try:
                child.wait(timeout=0.2)
            except subprocess.TimeoutExpired:
                pass
            else:
                raise AssertionError("Referenced event watch did not retain runtime")
            (work / "reference.txt").write_text("external-native-change")
            out, err = child.communicate(timeout=5)
            assert child.returncode == 0 and out == "REF_READY\nREF_CHANGED\n" and not err, (
                out,
                err,
                child.returncode,
            )
        finally:
            if child.poll() is None:
                child.kill()
                child.communicate(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    executables = [("Node", args.node)]
    if not args.reference_only:
        executables.append(("Mite", str(args.mite.resolve())))
    for fixture in [
        "main",
        "recursive",
        "abort",
        "validation",
        "unref",
        "unref-explicit",
        "policy",
    ]:
        observations = []
        for name, binary in executables:
            with tempfile.TemporaryDirectory(prefix="mite-event-watcher-") as work:
                script = "unref" if fixture == "unref-explicit" else fixture
                result = subprocess.run(
                    [
                        binary,
                        str(ROOT / "mite/tests/event-watcher" / (script + ".mjs")),
                        *(["explicit"] if fixture == "unref-explicit" else []),
                    ],
                    cwd=work,
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    timeout=10,
                )
            assert result.returncode == 0 and not result.stderr, (name, fixture, result)
            if fixture == "policy":
                assert result.stdout.endswith("DONE controlled event watcher policy\n"), result
            observations.append(result.stdout)
        assert all(value == observations[0] for value in observations), (fixture, observations)
        if observations[0].startswith("SKIP"):
            print(observations[0], end="", flush=True)
            continue
        print(
            (
                "PASS controlled event watcher factory/"
                if fixture == "policy"
                else "PASS real event watcher/"
            )
            + fixture
            + " ("
            + ", ".join(name for name, _ in executables)
            + ")",
            flush=True,
        )

    for mode in ["normal", "explicit"]:
        for name, binary in executables:
            referenced(binary, mode)
        print("PASS real event watcher/reference-" + mode, flush=True)


if __name__ == "__main__":
    main()
