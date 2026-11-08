#!/usr/bin/env python3
"""Observe a real current-document raw channel handoff in the production helper."""

import _support
import argparse
import os
from pathlib import Path
import re
import selectors
import shutil
import subprocess
import tempfile
import time
from test_renderer import ROOT, environment, stop

READY = re.compile(
    r"FIREFLY_IPC_BYTES_READY window=(\d+) epoch=(\d+) pid=(\d+) document=(\d+):(\d+)"
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-ipc-channel-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/ipc/outbound", root, dirs_exist_ok=True)
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        env = environment(str(root / "environment"))
        env["FIREFLY_IPC_BYTES_DIAGNOSTIC"] = "1"
        process = subprocess.Popen(
            [str(args.firefly.resolve()), str(root / "main.mjs"), "--binding-probe"],
            env=env,
            start_new_session=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        records = []
        output = {"stdout": bytearray(), "stderr": bytearray()}
        lines = {"stdout": "", "stderr": ""}
        checked_pids = set()
        try:
            deadline = time.monotonic() + 30
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
                        lines[key.data] += data.decode("utf-8", errors="replace")
                        while "\n" in lines[key.data]:
                            line, lines[key.data] = lines[key.data].split("\n", 1)
                            match = READY.fullmatch(line)
                            if not match:
                                continue
                            record = tuple(map(int, match.groups()))
                            records.append(record)
                            if record[2] not in checked_pids:
                                assert record[2] > 1 and record[2] != process.pid, record
                                parent = subprocess.check_output(
                                    ["ps", "-o", "ppid=", "-p", str(record[2])], text=True
                                )
                                assert int(parent.strip()) == process.pid, (
                                    record,
                                    parent,
                                    process.pid,
                                )
                                checked_pids.add(record[2])
            process.wait(timeout=5)
            assert process.returncode == 0 and checked_pids, output
            assert b"PASS public main-to-renderer events:" in output["stdout"], output
            assert b"VERIFICATION FAILED" not in output["stderr"], output
            assert b"ERR_FIREFLY_IPC_BYTES" not in output["stderr"], output
            assert len(records) == 3, (records, output)
            assert records[0][0] == records[1][0] and records[2][0] != records[0][0], records
            assert records[0][1] < records[1][1] < records[2][1], records
            assert len({record[3:] for record in records}) == 3, records
            print(
                "PASS native document IPC channel: real owned helper PID, exact epoch/document handshake, reload/multiwindow rebinding and teardown"
            )
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
