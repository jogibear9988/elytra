#!/usr/bin/env python3
"""Validate process.kill against independent, authenticated child lifetimes, never desktop PIDs."""

import argparse
import json
import os
from pathlib import Path
import queue
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "mite/tests/process-kill/main.mjs"


def reader(stream):
    lines = []
    events = queue.Queue()

    def read():
        try:
            for line in stream:
                lines.append(line)
                events.put(line)
        finally:
            events.put(None)

    thread = threading.Thread(target=read, daemon=True)
    thread.start()
    return lines, events, thread


def run(binary, mode, *, host=False):
    nonce = uuid.uuid4().hex

    victim = subprocess.Popen(
        [sys.executable, "-I", "-u", str(FIXTURE.with_name("victim.py")), nonce],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=os.name != "nt",
    )
    runtime = None
    threads = []
    with tempfile.TemporaryFile(mode="w+") as errors:
        try:
            _, readiness, target_reader = reader(victim.stdout)
            threads.append(target_reader)
            ready = json.loads(readiness.get(timeout=10))
            assert ready == {"pid": victim.pid, "parent": os.getpid(), "nonce": nonce}
            assert victim.pid > 0 and victim.poll() is None
            command = [str(binary), str(FIXTURE), str(victim.pid), str(os.getpid()), nonce, mode]
            runtime = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=errors,
                text=True,
                start_new_session=os.name != "nt",
            )
            output, records, runtime_reader = reader(runtime.stdout)
            threads.append(runtime_reader)
            first = records.get(timeout=10)
            assert first and json.loads(first) == {
                "phase": "ready",
                "target": victim.pid,
                "pid": runtime.pid,
                "nonce": nonce,
            }
            # No supplied PID/group is used. This Popen object still owns the
            # same live child which authenticated itself over its private pipe.
            assert victim.poll() is None
            runtime.stdin.write("g")
            runtime.stdin.flush()
            sent = records.get(timeout=10)
            assert sent and json.loads(sent) == {
                "phase": "sent",
                "target": victim.pid,
                "nonce": nonce,
            }
            status = victim.wait(timeout=5)
            assert status == (1 if os.name == "nt" else -signal.SIGTERM), status
            runtime.stdin.write("r")
            runtime.stdin.flush()
            runtime.stdin.close()
            runtime.wait(timeout=10)
            runtime_reader.join(timeout=2)
            errors.seek(0)
            error_output = errors.read(32768)
            assert runtime.returncode == 0 and error_output == (
                "HOST_SHUTDOWN\n" if host else ""
            ), (command, runtime.returncode, output, error_output)
            assert output[2:] == [
                "PASS process.kill owned OS delivery, coercion, zero and reaped ESRCH\n"
            ], output
        except Exception:
            errors.seek(0)
            detail = errors.read(32768)
            if detail:
                print(detail, file=sys.stderr)
            raise
        finally:
            # Cleanup uses only our retained direct child handles, not external
            # PID arguments or process groups. Python's Popen wait reaps both.
            for child in (runtime, victim):
                if child and child.poll() is None:
                    child.kill()
                if child:
                    child.wait(timeout=5)
            for thread in threads:
                thread.join(timeout=2)
            if runtime:
                for stream in (runtime.stdin, runtime.stdout):
                    if stream:
                        stream.close()
            victim.stdout.close()
            victim.stderr.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--host", type=Path)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    runtimes = [("Node", args.node, False)]
    if not args.reference_only:
        runtimes.append(("Mite", args.mite.resolve(), False))
        if args.host:
            runtimes.append(("Host", args.host.resolve(), True))
    for label, binary, host in runtimes:
        for mode in ("default", "named", "numeric", "false", "null", "empty"):
            run(binary, mode, host=host)
        print(f"PASS {label} process.kill 6 owned signal/coercion/liveness/reap cases", flush=True)


if __name__ == "__main__":
    main()
