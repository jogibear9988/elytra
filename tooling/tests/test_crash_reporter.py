#!/usr/bin/env python3
"""Capture an actual public Firefly main SIGABRT and recover its private native report; no windows."""

import _support

import argparse
import json
import os
from pathlib import Path
import sys

if sys.platform != "win32":
    import resource
import selectors
import signal
import stat
import subprocess
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform == "win32":
        parser.error("Native Windows crash capture/runtime is not implemented")
    with tempfile.TemporaryDirectory(prefix="firefly crash ä ") as temporary:
        root = Path(temporary)
        directory = root / "Reports 世界"
        directory.mkdir(mode=0o700)
        for mode in ["crash", "recover", "helper"]:
            command = [
                str(args.firefly.resolve()),
                "--electron-compat",
                str(ROOT / "firefly/tests/crash/main.mjs"),
                mode,
                str(directory),
            ]
            env = environment(str(root / "profile"))
            env["FIREFLY_CRASH_TEST_PRIVATE_SENTINEL"] = "SECRET_ENV_NOT_COLLECTED_94721"

            def no_core():
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

            process = subprocess.Popen(
                command,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                start_new_session=True,
                preexec_fn=no_core,
            )
            try:
                prefix = ""
                if mode in ("crash", "helper"):
                    marker = (
                        "CRASH_REPORTER_READY crash" if mode == "crash" else "CRASH_HELPER_READY "
                    )
                    with selectors.DefaultSelector() as selector:
                        selector.register(process.stdout, selectors.EVENT_READ)
                        while marker not in prefix:
                            assert selector.select(timeout=20), (
                                "No native crash readiness",
                                prefix,
                            )
                            chunk = os.read(process.stdout.fileno(), 8192)
                            assert chunk, (process.poll(), prefix)
                            prefix += chunk.decode("utf-8", errors="replace")
                    target = (
                        process.pid
                        if mode == "crash"
                        else int(prefix.split("CRASH_HELPER_READY ")[-1].strip())
                    )
                    if mode == "helper":
                        parent = subprocess.check_output(
                            ["ps", "-o", "ppid=", "-p", str(target)], text=True
                        ).strip()
                        assert parent == str(process.pid), (target, parent, process.pid)
                    os.kill(target, signal.SIGABRT)
                stdout, stderr = process.communicate(timeout=35)
                stdout = prefix + stdout
                assert process.returncode == (-signal.SIGABRT if mode == "crash" else 0), (
                    command,
                    process.returncode,
                    stdout,
                    stderr,
                )
                assert f"CRASH_REPORTER_READY {mode}" in stdout, (stdout, stderr)
                assert "VERIFICATION FAILED" not in stderr, stderr
            finally:
                stop(process)
                process.communicate()
            if mode == "crash":
                assert any(p.name.endswith(".pending") for p in directory.iterdir()), list(
                    directory.iterdir()
                )
        reports = list(directory.glob("*.txt"))
        assert len(reports) == 2, list(directory.iterdir())
        main_report = next(p for p in reports if "-Browser-" in p.name)
        helper_report = next(p for p in reports if "-WebContent-" in p.name)
        text = main_report.read_text()
        assert "SIGABRT" in text and "Native stack" in text, text
        metadata = [
            json.loads(line.removeprefix("Firefly application: "))
            for line in text.splitlines()
            if line.startswith("Firefly application: ")
        ]
        assert len(metadata) == 1 and metadata[0]["productName"] == "Crash Fixture One 世界🚀", text
        assert metadata[0]["annotations"] == {
            "scope": "main-only Grüße",
            "overridden": "global",
            "global": "all-helpers",
        }, text
        assert (
            "Recovery Fixture Two" not in text and "SECRET_ENV_NOT_COLLECTED_94721" not in text
        ), text
        assert str(root) not in text and str(ROOT) not in text, text
        child_text = helper_report.read_text()
        child_metadata = [
            json.loads(line.removeprefix("Firefly application: "))
            for line in child_text.splitlines()
            if line.startswith("Firefly application: ")
        ]
        assert (
            len(child_metadata) == 1 and child_metadata[0]["productName"] == "Helper Fixture Three"
        ), child_text
        assert child_metadata[0]["annotations"] == {
            "overridden": "global",
            "global": "all-helpers",
        }, child_text
        assert "main-only" not in child_text and "SIGABRT" in child_text, child_text
        assert "SECRET_ENV_NOT_COLLECTED_94721" not in child_text and str(root) not in child_text, (
            child_text
        )
        assert all(stat.S_IMODE(p.stat().st_mode) == 0o400 for p in reports), [
            p.stat() for p in reports
        ]
        assert stat.S_IMODE(directory.stat().st_mode) == 0o700, directory.stat()
        assert reports[0].stat().st_uid == os.getuid()
        assert not any(p.name.endswith(".pending") for p in directory.iterdir()), list(
            directory.iterdir()
        )
    print(
        "PASS actual public main/owned-helper crash capture, recovery, frozen/private metadata and clean-exit cleanup"
    )


if __name__ == "__main__":
    main()
