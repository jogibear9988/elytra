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


def verify_handoffs(records, rejected, output):
    # Two primary-document epochs, two simultaneous burst peers, then
    # the final binding-only window. Peer READY callbacks may race.
    assert len(records) == 5, (records, output)
    primary = records[0][0]
    assert records[1][0] == primary, records
    assert records[0][1] < records[1][1], records
    peers = {records[2][0], records[3][0]}
    assert len(peers) == 2 and primary not in peers, records
    assert records[4][0] not in peers | {primary}, records
    assert max(record[1] for record in records[:2]) < min(record[1] for record in records[2:4]), (
        records
    )
    assert max(record[1] for record in records[2:4]) < records[4][1], records
    assert len({record[1] for record in records}) == 5, records
    assert len({record[3:] for record in records}) == 5, records
    expected = {(window, epoch) for window, epoch, *_ in records}
    assert {(window, epoch) for window, epoch, _ in rejected} == expected, (
        records,
        rejected,
    )
    assert len(rejected) == 15, (records, rejected)
    for window, epoch in expected:
        assert sorted(case for w, e, case in rejected if (w, e) == (window, epoch)) == [
            0,
            1,
            2,
        ], (records, rejected, output)


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
        env["FIREFLY_IPC_BYTES_DIAGNOSTIC_ADMISSION"] = "1"
        process = subprocess.Popen(
            [str(args.firefly.resolve()), str(root / "main.mjs"), "--binding-probe"],
            env=env,
            start_new_session=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        records = []
        rejected = []
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
                            negative = re.fullmatch(
                                r"FIREFLY_IPC_BYTES_REJECTED window=(\d+) epoch=(\d+) case=(\d+)",
                                line,
                            )
                            if negative:
                                rejected.append(tuple(map(int, negative.groups())))
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
            assert b"BAD_HANDOFF_ACCEPTED" not in output["stderr"], output
            verify_handoffs(records, rejected, output)
            print(
                "PASS native document IPC channel: real owned helper PID, exact epoch/document handshake, reload/multiwindow rebinding and teardown"
            )
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
