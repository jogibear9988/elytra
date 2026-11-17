#!/usr/bin/env python3
"""Compare real owned headless PTYs with the unchanged pinned node-pty oracle."""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/pty"


def owned_alive(pid, nonce):
    """Authenticate the exact owned Python child before any cleanup signal."""
    result = subprocess.run(
        ["/bin/ps", "-ww", "-p", str(pid), "-o", "args="],
        capture_output=True,
        text=True,
        timeout=3,
    )
    return (
        result.returncode == 0
        and nonce in result.stdout
        and str(FIXTURES / "peer.py") in result.stdout
    )


def run(binary, backend, name="main.mjs", groups=6, child_count=3):
    with tempfile.TemporaryDirectory(prefix="mite-pty-") as temporary:
        directory = Path(temporary).resolve()
        record = directory / "children.jsonl"
        nonce = uuid.uuid4().hex
        command = [
            str(binary),
            "--expose-gc",
            str(FIXTURES / name),
            backend,
            sys.executable,
            str(directory),
            nonce,
            str(record),
        ]
        result = None
        try:
            with subprocess.Popen(
                command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=ROOT
            ) as process:
                owner_pid = process.pid
                try:
                    output, errors = process.communicate(timeout=40)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate(timeout=3)
                    raise
                result = subprocess.CompletedProcess(command, process.returncode, output, errors)
        finally:
            records = (
                [json.loads(line) for line in record.read_text().splitlines()]
                if record.exists()
                else []
            )
            leaked = []
            for child in records:
                assert child["nonce"].startswith(nonce + "-")
                if owned_alive(child["pid"], child["nonce"]):
                    leaked.append(child["pid"])
                    os.kill(child["pid"], signal.SIGKILL)
            assert not leaked, (
                f"Owned PTY children leaked (authenticated cleanup performed): {leaked}"
            )
        assert result.returncode == 0 and not result.stderr, result
        lines = result.stdout.splitlines()
        assert len(lines) == groups and all(line.startswith("PASS ") for line in lines), result
        assert len(records) == child_count
        for child in records:
            assert child["parent"] == owner_pid
            assert child["tty"] == [True, True, True]
            assert child["pid"] == child["group"] == child["session"] == child["foreground"]
            assert child["cwd"] == str(directory)
            assert child["term"] == "xterm-256color"
        return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument(
        "--node-pty",
        type=Path,
        help="Existing pinned node-pty package directory for optional Node oracle",
    )
    parser.add_argument("--mite", type=Path)
    parser.add_argument(
        "--mite-node-pty", type=Path, help="Exercise the adapted real package entry"
    )
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if os.name == "nt":
        parser.error(
            "This POSIX provider fixture requires real /dev/tty; Windows acceptance is not implemented"
        )
    if args.reference_only and (not args.node or not args.node_pty):
        parser.error("--reference-only requires Node and --node-pty")
    if not args.reference_only and not args.mite:
        parser.error("--mite required without --reference-only")
    reference = None
    if args.node_pty:
        package = args.node_pty.resolve()
        manifest = json.loads((package / "package.json").read_text())
        assert manifest["name"] == "node-pty" and manifest["version"] == "1.2.0-beta.18"
        reference = run(args.node, str(package))
        print(reference, end="", flush=True)
    if not args.reference_only:
        backend = str(args.mite_node_pty.resolve()) if args.mite_node_pty else "mite:pty"
        native = run(args.mite.resolve(), backend)
        policy = run(args.mite.resolve(), "mite:pty", "policy.mjs", 2, 1)
        shutdown = run(args.mite.resolve(), "mite:pty", "shutdown.mjs", 1, 1)
        print(policy + shutdown, end="", flush=True)
        if reference is not None:
            assert native == reference
            print("PASS real native PTY differential and independent child cleanup")
        else:
            print(native, end="", flush=True)
            print(
                "PASS native PTY and independent child cleanup (no Node package oracle requested)"
            )


if __name__ == "__main__":
    main()
