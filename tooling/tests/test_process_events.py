#!/usr/bin/env python3
"""Exercise process events and real POSIX delivery to exclusively owned child PIDs."""

import _support

import argparse
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/process-events"


def run(binary, fixture, *args, env=None, host=False, options=()):
    result = subprocess.run(
        [str(binary), *options, str(FIXTURES / fixture), *args],
        capture_output=True,
        text=True,
        timeout=15,
        env=env,
    )
    assert result.returncode == 0, result
    assert result.stderr == ("HOST_SHUTDOWN\n" if host else ""), result
    assert result.stdout and all(line.startswith("PASS ") for line in result.stdout.splitlines()), (
        result
    )
    return result.stdout


def delivery(binary, name, *, host=False, ignore=False):
    env = dict(os.environ, MITE_TEST_ALLOW_SIGNALS="1")
    if ignore:
        env["MITE_TEST_PROTECT_SIGNAL"] = "ignore"
    child = subprocess.Popen(
        [str(binary), str(FIXTURES / "signal.mjs"), name],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        start_new_session=True,
    )
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(child.stdout, selectors.EVENT_READ)
            for phase in [1, 2]:
                assert ready.select(10), (name, phase, "readiness deadline")
                record = json.loads(child.stdout.readline())
                assert record == {"ready": phase, "pid": child.pid, "type": name}, record
                # Never signal the desktop, parent, a process group, or a supplied PID.
                assert child.pid > 0 and child.poll() is None
                os.kill(child.pid, getattr(signal, name))
        out, err = child.communicate(timeout=10)
        assert child.returncode == 0, (name, child.returncode, out, err)
        assert out == b"PASS two native signals/once/microtasks/cleanup\n", out
        assert err == (b"HOST_SHUTDOWN\n" if host else b""), err
    finally:
        if child.poll() is None:
            child.kill()
        child.communicate(timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node, "Node oracle required"
    mite, host = args.mite.resolve(), args.host.resolve()
    reference = run(args.node, "custom.mjs", options=("--expose-gc",))
    assert reference == run(mite, "custom.mjs") == run(host, "custom.mjs", host=True)
    print(reference, end="", flush=True)
    print(run(mite, "policy.mjs", "cli"), end="", flush=True)
    env = {key: value for key, value in os.environ.items() if not key.startswith("MITE_TEST_")}
    print(run(host, "policy.mjs", "host", host=True, env=env), end="", flush=True)
    # Watchers are unreferenced: a subscription alone must not keep either runtime alive.
    for binary in [args.node, mite]:
        result = subprocess.run(
            [str(binary), "-e", 'process.on("SIGINT",()=>{});'], capture_output=True, timeout=10
        )
        assert result.returncode == 0 and not result.stdout and not result.stderr, result
    if os.name != "nt":
        for name in ["SIGINT", "SIGTERM", "SIGPIPE", "SIGHUP", "SIGUSR2"]:
            for binary in [args.node, mite]:
                delivery(binary, name)
            print(f"PASS Node/Mite actual {name} delivery", flush=True)
        delivery(host, "SIGUSR2", host=True)
        delivery(host, "SIGUSR2", host=True, ignore=True)
        for mode in ["protected", "ignore"]:
            protected = dict(env, MITE_TEST_ALLOW_SIGNALS="1", MITE_TEST_PROTECT_SIGNAL=mode)
            print(run(host, "policy.mjs", mode, host=True, env=protected), end="", flush=True)
        print("PASS embedded signal delivery/GC and native handler restoration", flush=True)
    else:
        print("POSIX signal delivery not applicable on Windows; registration/unref/policy only")


if __name__ == "__main__":
    main()
