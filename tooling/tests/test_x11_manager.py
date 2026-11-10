#!/usr/bin/env python3
"""Non-GUI controls for Openbox startup ordering and independent readiness admission."""

import _support

from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import _x11


class Process:
    def __init__(self, pid):
        self.pid = pid
        self.returncode = None
        self.waited = False

    def poll(self):
        return self.returncode

    def wait(self, timeout):
        self.waited = True
        return self.returncode


class ManagerOrdering(unittest.TestCase):
    def exercise(self, mode):
        """Model only processes/X observations; execute the real non-GUI marker command."""
        manager, control = Process(123456701), Process(123456702)
        calls = []
        command = None
        marker = None
        root_calls = 0
        startup_wait_done = False
        real_run = subprocess.run
        real_popen = subprocess.Popen

        def launch(args, **kwargs):
            nonlocal command, marker
            calls.append(args[0])
            if args[0] == "openbox":
                self.assertEqual(args[:3], ["openbox", "--sm-disable", "--startup"])
                command = shlex.split(args[3])
                self.assertEqual(command[0], sys.executable)
                marker = Path(command[-1])
                self.assertFalse(marker.exists())
                return manager
            self.assertTrue(startup_wait_done, "Control launched before startup barrier")
            self.assertEqual(marker.read_bytes(), b"ready\n")
            return control

        def root(*args):
            nonlocal root_calls
            root_calls += 1
            return "" if root_calls == 1 else "0x123"

        def wait(predicate, message):
            nonlocal startup_wait_done
            if "startup callback" in message:
                # A published WM root property must not admit the control early.
                self.assertFalse(predicate())
                self.assertEqual(calls, ["openbox"])
                if mode == "missing":
                    raise AssertionError("missing startup callback")
                if mode == "exited":
                    manager.returncode = 7
                    predicate()
                    self.fail("Exited manager admitted")
                with patch.object(subprocess, "Popen", real_popen):
                    result = real_run(command, capture_output=True, text=True, timeout=5)
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, "", ""))
                self.assertTrue(predicate())
                startup_wait_done = True
            else:
                if not predicate():
                    raise AssertionError(message)

        def stop(process):
            process.returncode = -9

        env = {
            "DISPLAY": ":123",
            "QT_QPA_PLATFORM": "xcb",
            "FIREFLY_TEST_ISOLATED_X11": "1",
        }
        with tempfile.TemporaryDirectory(prefix="firefly-x11-manager-unit-") as directory:
            with ExitStack() as stack:
                for target, value in [
                    ("sys.platform", "linux"),
                    ("subprocess.Popen", launch),
                    ("root_property", root),
                    ("wait_for", wait),
                    ("stop", stop),
                    (
                        "query",
                        lambda *a, **k: "Window id: 0x456\nMap State: "
                        + ("IsUnMapped" if mode == "unmapped" else "IsViewable"),
                    ),
                    (
                        "property_values",
                        lambda *a, **k: [] if mode == "undecorated" else [1, 1, 20, 1],
                    ),
                    ("process_state", lambda p: {"returncode": p.poll()}),
                ]:
                    stack.enter_context(
                        patch.object(_x11, target, value)
                        if "." not in target
                        else patch("_x11." + target, value)
                    )
                stack.enter_context(redirect_stdout(io.StringIO()))
                stack.enter_context(redirect_stderr(io.StringIO()))
                if mode == "ready":
                    with _x11.managed_window_manager(env, Path(directory) / "manager.log"):
                        self.assertEqual(calls, ["openbox", "xmessage"])
                        self.assertTrue(control.waited)
                else:
                    with self.assertRaises(AssertionError):
                        with _x11.managed_window_manager(env, Path(directory) / "manager.log"):
                            self.fail("Invalid manager/control admitted")
        self.assertTrue(manager.waited)
        if mode in ("missing", "exited"):
            self.assertEqual(calls, ["openbox"])
        else:
            self.assertTrue(control.waited)
        self.assertFalse(marker.exists(), "Owned startup marker leaked")

    def test_ready_control_runs_after_real_marker(self):
        self.exercise("ready")

    def test_advertisement_alone_never_launches_control(self):
        self.exercise("missing")

    def test_exited_manager_never_launches_control(self):
        self.exercise("exited")

    def test_unmapped_control_still_rejected(self):
        self.exercise("unmapped")

    def test_missing_decorations_still_rejected(self):
        self.exercise("undecorated")


if __name__ == "__main__":
    unittest.main()
