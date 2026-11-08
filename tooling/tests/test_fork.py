#!/usr/bin/env python3
"""Compare real runtime self-fork/IPC against Node, with bounded test-owned children.

This gate never launches a GUI. The independent-child case uses an acknowledgement
written only after its parent exits, so child survival cannot be confused with
capture-pipe inheritance or a parent waiting for its child to terminate.
"""

import _support

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/fork"


def run(executable, fixture, directory):
    result = subprocess.run(
        [executable, str(FIXTURES / (fixture + ".mjs"))],
        cwd=directory,
        env={**os.environ, "NODE_OPTIONS": ""},
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=20,
    )
    assert result.returncode == 0, result
    if fixture == "lifetime":
        assert result.stderr == "FORK_STDERR\n", result
        assert result.stdout.startswith("FORK_STDOUT\n"), result
    else:
        assert not result.stderr, result
    return result.stdout


def independent(executable, directory, label):
    marker = directory / (label + "-survived")
    ack = directory / (label + "-ack")
    parent = subprocess.Popen(
        [executable, str(FIXTURES / "independent.mjs"), str(marker), str(ack)],
        cwd=directory,
        env={**os.environ, "NODE_OPTIONS": ""},
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    try:
        parent.wait(timeout=3)
        assert parent.returncode == 0, parent.returncode
        out, err = parent.communicate(timeout=1)
        assert out == "PARENT_DONE\n" and not err, (out, err)
        assert not marker.exists(), "Child acknowledgement predates parent exit"
    finally:
        # Always release the owned child, even when parent-exit acceptance fails.
        # Its own five-second deadline also bounds cleanup after harness failure.
        ack.touch()
        if parent.poll() is None:
            parent.kill()
            parent.communicate(timeout=5)
    deadline = time.monotonic() + 5
    while not marker.exists() and time.monotonic() < deadline:
        time.sleep(0.02)
    assert marker.read_text() == "CHILD_SURVIVED", marker


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
    with tempfile.TemporaryDirectory(prefix="mite-fork-") as directory:
        work = Path(directory) / "cwd with spaces é文"
        work.mkdir()
        for fixture in ["main", "validation", "lifetime", "failure"]:
            results = [run(binary, fixture, work) for _, binary in executables]
            assert all(value == results[0] for value in results), (fixture, results)
            print(
                "PASS actual self-fork/"
                + fixture
                + " ("
                + ", ".join(name for name, _ in executables)
                + ")",
                flush=True,
            )
        for name, binary in executables:
            policy = run(binary, "channel-policy", work)
            assert policy == "PASS controlled IPC factory automatic/explicit reference policy\n", (
                policy
            )
            print("PASS " + name + " controlled IPC factory reference policy", flush=True)
            independent(binary, work, name)
            print(
                "PASS " + name + " parent exit preserves independently referenced fork child",
                flush=True,
            )
        if not args.reference_only:
            # An environment marker alone is not an IPC capability, and the
            # explicit internal role must refuse a missing/invalid inherited fd.
            mite = str(args.mite.resolve())
            for marker in [None, "0", "4", "3"]:
                environment = {**os.environ, "NODE_OPTIONS": ""}
                environment.pop("MITE_FORK_CHANNEL", None)
                if marker is not None:
                    environment["MITE_FORK_CHANNEL"] = marker
                rejected = subprocess.run(
                    [mite, "--mite-fork-child", str(FIXTURES / "child.mjs")],
                    cwd=work,
                    env=environment,
                    capture_output=True,
                    text=True,
                    close_fds=True,
                    timeout=5,
                )
                assert rejected.returncode == 9 and "fork-child" in rejected.stderr, rejected
            ordinary = subprocess.run(
                [mite, "-e", "console.log(typeof process.send)"],
                cwd=work,
                env={**os.environ, "MITE_FORK_CHANNEL": "3", "NODE_OPTIONS": ""},
                capture_output=True,
                text=True,
                timeout=5,
            )
            assert (
                ordinary.returncode == 0
                and ordinary.stdout == "undefined\n"
                and not ordinary.stderr
            ), ordinary
            print("PASS Mite private fork bootstrap refuses missing/invalid capability", flush=True)
            result = run(str(args.mite.resolve()), "policy", work)
            assert result == "PASS explicit unsupported fork modes\n", result
            print(result, end="", flush=True)


if __name__ == "__main__":
    main()
