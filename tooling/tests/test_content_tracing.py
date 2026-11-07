#!/usr/bin/env python3
"""Inspect real native duration traces produced by the public Firefly main API."""

import _support

import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import tempfile
from test_renderer import ROOT, run
from unittest.mock import patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-trace-public-") as directory:
        with patch.dict(os.environ, {"FIREFLY_TEST_SECRET": "trace-private-environment-sentinel"}):
            result = run(
                args.firefly.resolve(),
                "--electron-compat",
                ROOT / "firefly/tests/tracing/main.mjs",
                directory,
            )
        assert result.returncode == 0, result
        assert "PASS actual public main tracing" in result.stdout, result
        record = next(
            json.loads(line)
            for line in result.stdout.splitlines()
            if line.startswith('{"categories"')
        )
        explicit = Path(record["explicit"])
        temporary = Path(record["temporary"])
        try:
            assert explicit == Path(directory) / "explicit.json", record
            assert temporary.name == "trace.json" and temporary.parent.name.startswith(
                "firefly-trace-"
            ), record
            for file in (explicit, temporary):
                assert file.is_file() and stat.S_IMODE(file.stat().st_mode) == 0o600, file
                raw = file.read_text()
                assert "trace-private-environment-sentinel" not in raw and str(ROOT) not in raw, raw
                trace = json.loads(raw)
                assert trace["metadata"]["scope"] == "main-process-only", trace
                assert trace["metadata"]["droppedEvents"] == 0, trace
                assert all(
                    event["pid"] == result.pid and event["tid"] == 1
                    for event in trace["traceEvents"]
                ), trace
            trace = json.loads(explicit.read_text())
            measured = [event for event in trace["traceEvents"] if event["ph"] == "X"]
            assert measured and len(measured) <= 8192, trace
            assert all(
                event["cat"] == "firefly.main"
                and event["name"] == "event-loop-turn"
                and event["ts"] > 0
                and event["dur"] >= 0
                for event in measured
            ), measured
            assert all(
                event["ph"] == "M" for event in json.loads(temporary.read_text())["traceEvents"]
            ), record
            assert stat.S_IMODE(temporary.parent.stat().st_mode) == 0o700, temporary.parent
        finally:
            if (
                temporary.parent.name.startswith("firefly-trace-")
                and temporary.name == "trace.json"
            ):
                shutil.rmtree(temporary.parent)
        print(result.stdout.strip())
        print(
            "PASS native duration/identity, exact filtering, private files and no recorded application payload"
        )


if __name__ == "__main__":
    main()
