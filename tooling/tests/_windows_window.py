"""Observe native Windows pipes/HWNDs and crash only the verified owned test renderer."""

import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import os
import re
import time

from _windows_processes import WindowsProcesses


def loaded_renderer(process, timeout=15):
    native = WindowsProcesses()
    import msvcrt

    peek = native.kernel.PeekNamedPipe
    peek.argtypes = [
        W.HANDLE,
        C.c_void_p,
        W.DWORD,
        C.POINTER(W.DWORD),
        C.POINTER(W.DWORD),
        C.POINTER(W.DWORD),
    ]
    peek.restype = W.BOOL
    pipe = msvcrt.get_osfhandle(process.stdout.fileno())
    pending = b""
    received = 0
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        available = W.DWORD()
        if not peek(pipe, None, 0, None, C.byref(available), None):
            code = C.get_last_error()
            if code in (109, 232):  # Broken/no-data pipe, not a fabricated PID.
                raise AssertionError("Window probe closed its pipe before renderer readiness")
            raise C.WinError(code)
        if available.value:
            chunk = os.read(process.stdout.fileno(), min(65536, available.value))
            received += len(chunk)
            pending += chunk
            if received > 1024 * 1024:
                raise AssertionError("Window startup output exceeded its one-MiB budget")
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                match = re.fullmatch(rb"WebContent PID: (\d+)\r?", line)
                if match:
                    return int(match[1])
        if process.poll() is not None:
            raise AssertionError("Window probe exited before renderer readiness")
        time.sleep(0.02)
    raise AssertionError("Native window probe did not report a loaded renderer")


def visible_windows(pid):
    # Observe real HWNDs independently of Qt's platform-name/self-test markers.
    WindowsProcesses()  # Fail before native API loading on unsupported hosts.
    user = C.WinDLL("user32", use_last_error=True)
    callback_type = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    user.EnumWindows.argtypes = [callback_type, W.LPARAM]
    user.EnumWindows.restype = W.BOOL
    user.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
    user.GetWindowThreadProcessId.restype = W.DWORD
    user.IsWindowVisible.argtypes = [W.HWND]
    user.IsWindowVisible.restype = W.BOOL
    user.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
    user.GetClientRect.restype = W.BOOL
    windows, failures = [], []

    @callback_type
    def visit(window, _):
        try:
            owner = W.DWORD()
            if (
                user.GetWindowThreadProcessId(window, C.byref(owner))
                and owner.value == pid
                and user.IsWindowVisible(window)
            ):
                bounds = W.RECT()
                if not user.GetClientRect(window, C.byref(bounds)):
                    raise C.WinError(C.get_last_error())
                width, height = bounds.right - bounds.left, bounds.bottom - bounds.top
                if width > 0 and height > 0:
                    windows.append(
                        {"hwnd": int(window), "pid": pid, "width": width, "height": height}
                    )
            return True
        except Exception as error:
            failures.append(error)
            return False

    succeeded = user.EnumWindows(visit, 0)
    if failures:
        raise failures[0]
    if not succeeded:
        raise C.WinError(C.get_last_error())
    return windows


def crash_renderer(probe, process, pid):
    native = WindowsProcesses()
    assert process.poll() is None, "Window probe exited before ownership verification"
    windows = visible_windows(process.pid)
    assert windows, "No independently observed visible native HWND for the window probe"
    print(f"Observed native Windows HWNDs: {windows}")
    assert native.children(process.pid).get(pid, "").casefold() == "webcontent.exe", pid
    expected = (probe.parent / "WebContent.exe").resolve(strict=True)
    observed = native.inspect(pid)
    assert Path(observed["image"]) == expected, observed
    kernel = native.kernel
    kernel.TerminateProcess.argtypes = [W.HANDLE, W.UINT]
    kernel.TerminateProcess.restype = W.BOOL
    # Request terminate rights only here, after native parent/image verification.
    # Holding this exact handle and matching creation time prevents PID-reuse kills.
    with native.handle(kernel.OpenProcess(0x1000 | 0x100000 | 1, False, pid)) as handle:
        current = native.inspect_handle(handle, pid)
        assert current["creationTicks"] == observed["creationTicks"], (current, observed)
        assert Path(current["image"]) == expected and process.poll() is None, current
        if not kernel.TerminateProcess(handle, 1):
            raise C.WinError(C.get_last_error())
        assert kernel.WaitForSingleObject(handle, 5000) == 0, "Owned renderer did not terminate"
