#!/usr/bin/env python3
"""Run real no-window utility children and native parent IPC, optionally comparing Electron."""

import _support

import argparse
from pathlib import Path
import os
import signal
import subprocess
import tempfile
import time
from test_renderer import ROOT, environment, run, stop


class ChildIdentity:
    """Keep a process identity, so cleanup never targets a reused numeric PID."""

    def __init__(self, pid):
        self.pid = pid
        self.handle = None
        if os.name == "nt":
            import ctypes
            from ctypes import wintypes

            self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
            self.kernel.OpenProcess.restype = wintypes.HANDLE
            self.kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
            self.kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
            self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
            self.handle = self.kernel.OpenProcess(0x100000 | 0x1000 | 0x0001, False, pid)
            assert self.handle, "Utility child must exist while parent is alive"
        else:
            self.identity = self.snapshot()
            assert self.identity, "Utility child must exist while parent is alive"

    def snapshot(self):
        proc_stat = Path("/proc") / str(self.pid) / "stat"
        if Path("/proc").exists():
            try:
                # Linux's start ticks remain stable even after exit, until waitpid reaps it.
                return proc_stat.read_text().rsplit(") ", 1)[1].split()[19]
            except FileNotFoundError:
                return ""
        # macOS lstart remains stable for zombies too; command text becomes "<defunct>".
        result = subprocess.run(
            ["ps", "-p", str(self.pid), "-o", "lstart="],
            capture_output=True,
            text=True,
            timeout=3,
        )
        return result.stdout.strip() if result.returncode == 0 else ""

    def alive(self):
        if self.handle:
            return self.kernel.WaitForSingleObject(self.handle, 0) == 258
        return self.snapshot() == self.identity

    def cleanup(self):
        if self.alive():
            if self.handle:
                self.kernel.TerminateProcess(self.handle, 1)
            else:
                try:
                    os.kill(self.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        if self.handle:
            self.kernel.CloseHandle(self.handle)


def parent_teardown(binary, script, pid_file, directory):
    """Observe a live child before app.exit(), then require native parent cleanup."""
    process = subprocess.Popen(
        [str(binary.resolve()), str(script), str(pid_file)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=environment(str(directory)),
        start_new_session=os.name != "nt",
    )
    child = None
    try:
        deadline = time.monotonic() + 6
        pid = None
        while process.poll() is None and time.monotonic() < deadline:
            try:
                pid = int(pid_file.read_text())
                break
            except (FileNotFoundError, ValueError):
                time.sleep(0.02)
        assert pid is not None, "Parent did not report an actually spawned utility child"
        child = ChildIdentity(pid)
        assert process.poll() is None and child.alive(), "Observe child before parent exit"
        stdout, stderr = process.communicate(timeout=6)
        assert process.returncode == 0 and "PASS utility parent exiting" in stdout, (stdout, stderr)
        deadline = time.monotonic() + 2
        while child.alive() and time.monotonic() < deadline:
            time.sleep(0.02)
        # Check before process-group cleanup, which could otherwise hide an orphan.
        assert not child.alive(), "app.exit() left the owned utility child alive or unreaped"
    finally:
        stop(process)
        if child:
            child.cleanup()
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.firefly and not args.electron:
        parser.error("Provide --firefly and/or --electron")
    with tempfile.TemporaryDirectory(prefix="elytra-utility-") as directory:
        root = Path(directory)
        source = (ROOT / "firefly/tests/utility-process/main.mjs").read_text()
        teardown = (ROOT / "firefly/tests/utility-process/parent-exit.mjs").read_text()
        (root / "child.mjs").write_bytes(
            (ROOT / "firefly/tests/utility-process/child.mjs").read_bytes()
        )
        for name, binary in [("Firefly", args.firefly), ("Electron reference", args.electron)]:
            if binary is None:
                continue
            main = root / ("electron.mjs" if name == "Electron reference" else "firefly.mjs")
            main.write_text(
                source.replace("from 'firefly'", "from 'electron'")
                if name == "Electron reference"
                else source
            )
            result = run(binary.resolve(), main)
            assert result.returncode == 0 and "PASS public utilityProcess:" in result.stdout, result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(name + ": " + result.stdout.strip(), flush=True)
            teardown_main = root / (
                "electron-teardown.mjs" if name == "Electron reference" else "firefly-teardown.mjs"
            )
            teardown_main.write_text(
                teardown.replace("from 'firefly'", "from 'electron'")
                if name == "Electron reference"
                else teardown
            )
            parent_teardown(binary, teardown_main, root / (name + ".pid"), root)
            print(name + ": PASS parent exit reaps actual live utility child", flush=True)


if __name__ == "__main__":
    main()
