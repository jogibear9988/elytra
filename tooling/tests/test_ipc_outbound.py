#!/usr/bin/env python3
"""Batch real main-to-isolated-preload event delivery and document replacement."""

import _support
import argparse
from pathlib import Path
import shutil
import json
import os
import selectors
import signal
import time
import subprocess
import tempfile
from test_renderer import ROOT, run, environment, stop


def stalled_peer(binary, root):
    """Pause only an OS-verified owned renderer; always reap the test process group."""
    marker = root / "paused"
    command = [str(binary), str(root / "main.mjs"), "--pause-file=" + str(marker)]
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment(str(root / "environment")),
        start_new_session=True,
    )
    output = {"stdout": bytearray(), "stderr": bytearray()}
    pending = ""
    paused = None
    deadline = time.monotonic() + 80

    def owned(pid):
        assert pid > 1 and pid != process.pid
        parent = subprocess.check_output(["ps", "-o", "ppid=", "-p", str(pid)], text=True).strip()
        assert int(parent) == process.pid, (pid, parent, process.pid)

    try:
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ, "stdout")
            selector.register(process.stderr, selectors.EVENT_READ, "stderr")
            while selector.get_map():
                assert time.monotonic() < deadline, output
                for key, _ in selector.select(timeout=1):
                    data = os.read(key.fileobj.fileno(), 8192)
                    if not data:
                        selector.unregister(key.fileobj)
                        continue
                    output[key.data].extend(data)
                    if key.data != "stdout":
                        continue
                    pending += data.decode("utf-8")
                    while "\n" in pending:
                        line, pending = pending.split("\n", 1)
                        if line.startswith('{"pause":'):
                            paused = int(json.loads(line)["pause"])
                            owned(paused)
                            os.kill(paused, signal.SIGSTOP)
                            marker.write_text("paused")
                        if line == "IPC_TIMEOUT_RETIRED":
                            assert paused is not None
                            owned(paused)
                            os.kill(paused, signal.SIGCONT)
                            paused = None
                            Path(str(marker) + ".resumed").write_text("resumed")
        process.wait(timeout=5)
        result = subprocess.CompletedProcess(
            command,
            process.returncode,
            output["stdout"].decode("utf-8"),
            output["stderr"].decode("utf-8"),
        )
        assert "IPC_TIMEOUT_RETIRED" in result.stdout, result
        assert "endpoint retired" in result.stderr, result
        return result
    finally:
        stop(process)
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    parser.add_argument(
        "--electron",
        type=Path,
        help="Run the common contract in the real Electron reference instead",
    )
    parser.add_argument(
        "--functional-only",
        action="store_true",
        help="Skip the already-separate stalled-peer stress phase for a quick functional rerun; CI uses the full default gate",
    )
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-ipc-outbound-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/ipc/outbound", root, dirs_exist_ok=True)
        if args.electron:
            for name in ["main.mjs", "preload.mjs", "page.html"]:
                source = root / name
                source.write_text(
                    source.read_text()
                    .replace("from 'firefly'", "from 'electron'")
                    .replace("const limited = true;", "const limited = false;")
                    .replace("const stableIdentity = true;", "const stableIdentity = false;")
                )
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        if args.electron:
            result = run(
                args.electron.resolve(),
                "--user-data-dir=" + str(root / "profile"),
                root / "main.mjs",
                "--reference",
            )
            expected = "PASS Electron main-to-renderer event reference:"
        else:
            result = (
                run(args.firefly.resolve(), root / "main.mjs")
                if args.functional_only
                else stalled_peer(args.firefly.resolve(), root)
            )
            expected = "PASS public main-to-renderer events:"
        assert result.returncode == 0 and expected in result.stdout, result
        assert (
            "VERIFICATION FAILED" not in result.stderr
            and "ERR_FIREFLY_IPC_LISTENER" not in result.stderr
        ), result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
