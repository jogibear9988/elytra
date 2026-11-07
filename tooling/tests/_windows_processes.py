"""Read native Windows process identity and primary-token facts without changing privileges.

This is a test peer, independent of LibCore/LibWebView and page-supplied metadata.
Job membership and token observations alone never establish renderer containment.
"""

from contextlib import contextmanager
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import sys


class WindowsProcesses:
    def __init__(self):
        if sys.platform != "win32":
            raise RuntimeError("Native Windows process observations require Windows")
        self.kernel = C.WinDLL("kernel32", use_last_error=True)
        self.security = C.WinDLL("advapi32", use_last_error=True)

        class Entry(C.Structure):
            _fields_ = [
                ("size", W.DWORD),
                ("usage", W.DWORD),
                ("pid", W.DWORD),
                ("heap", C.c_size_t),
                ("module", W.DWORD),
                ("threads", W.DWORD),
                ("parent", W.DWORD),
                ("priority", W.LONG),
                ("flags", W.DWORD),
                ("name", W.WCHAR * 260),
            ]

        self.Entry = Entry
        signatures = [
            (self.kernel.CreateToolhelp32Snapshot, [W.DWORD, W.DWORD], W.HANDLE),
            (self.kernel.Process32FirstW, [W.HANDLE, C.POINTER(Entry)], W.BOOL),
            (self.kernel.Process32NextW, [W.HANDLE, C.POINTER(Entry)], W.BOOL),
            (self.kernel.CloseHandle, [W.HANDLE], W.BOOL),
            (self.kernel.OpenProcess, [W.DWORD, W.BOOL, W.DWORD], W.HANDLE),
            (self.kernel.WaitForSingleObject, [W.HANDLE, W.DWORD], W.DWORD),
            (
                self.kernel.QueryFullProcessImageNameW,
                [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)],
                W.BOOL,
            ),
            (self.kernel.GetProcessTimes, [W.HANDLE, *[C.POINTER(W.FILETIME)] * 4], W.BOOL),
            (self.kernel.IsProcessInJob, [W.HANDLE, W.HANDLE, C.POINTER(W.BOOL)], W.BOOL),
            (self.security.OpenProcessToken, [W.HANDLE, W.DWORD, C.POINTER(W.HANDLE)], W.BOOL),
            (
                self.security.GetTokenInformation,
                [W.HANDLE, C.c_int, C.c_void_p, W.DWORD, C.POINTER(W.DWORD)],
                W.BOOL,
            ),
            (self.security.IsValidSid, [C.c_void_p], W.BOOL),
            (self.security.GetSidSubAuthorityCount, [C.c_void_p], C.POINTER(W.BYTE)),
            (self.security.GetSidSubAuthority, [C.c_void_p, W.DWORD], C.POINTER(W.DWORD)),
        ]
        for function, arguments, result in signatures:
            function.argtypes, function.restype = arguments, result

    @contextmanager
    def handle(self, value):
        if not value or value == C.c_void_p(-1).value:
            raise C.WinError(C.get_last_error())
        try:
            yield value
        finally:
            if not self.kernel.CloseHandle(value):
                raise C.WinError(C.get_last_error())

    def children(self, parent):
        result = {}
        with self.handle(self.kernel.CreateToolhelp32Snapshot(2, 0)) as snapshot:
            entry = self.Entry()
            entry.size = C.sizeof(entry)
            found = self.kernel.Process32FirstW(snapshot, C.byref(entry))
            while found:
                if entry.parent == parent:
                    result[int(entry.pid)] = str(entry.name)
                found = self.kernel.Process32NextW(snapshot, C.byref(entry))
            if C.get_last_error() != 18:  # ERROR_NO_MORE_FILES, not an ignored API failure.
                raise C.WinError(C.get_last_error())
        return result

    def token_buffer(self, token, information):
        size = W.DWORD()
        self.security.GetTokenInformation(token, information, None, 0, C.byref(size))
        if C.get_last_error() != 122 or not 0 < size.value <= 1024 * 1024:
            raise C.WinError(C.get_last_error())
        data = C.create_string_buffer(size.value)
        if not self.security.GetTokenInformation(token, information, data, size, C.byref(size)):
            raise C.WinError(C.get_last_error())
        return data

    def token_number(self, token, information):
        data = self.token_buffer(token, information)
        if len(data) != C.sizeof(W.DWORD):
            raise ValueError(
                f"Unexpected native scalar token layout: class={information}, size={len(data)}"
            )
        return W.DWORD.from_buffer(data).value

    def token_filtered(self, token):
        # Class 21 is BOOLEAN at a DWORD-aligned address, despite older DWORD
        # documentation: microsoft/win32metadata discussion 2308. Keep capacity
        # four, require the actual one-byte result and consume only that byte.
        data = W.DWORD()
        returned = W.DWORD()
        if not self.security.GetTokenInformation(
            token, 21, C.byref(data), C.sizeof(data), C.byref(returned)
        ):
            raise C.WinError(C.get_last_error())
        if returned.value != C.sizeof(W.BYTE):
            raise ValueError(f"Unexpected TokenHasRestrictions size: {returned.value}")
        return C.cast(C.byref(data), C.POINTER(W.BYTE))[0] != 0

    def token(self, process):
        token = W.HANDLE()
        if not self.security.OpenProcessToken(process, 8, C.byref(token)):  # TOKEN_QUERY only.
            raise C.WinError(C.get_last_error())
        with self.handle(token.value) as token:
            if self.token_number(token, 8) != 1:  # TokenType == TokenPrimary.
                raise ValueError("Expected the actual process primary token")
            data = self.token_buffer(token, 25)  # TokenIntegrityLevel / TOKEN_MANDATORY_LABEL.
            if len(data) < C.sizeof(C.c_void_p):
                raise ValueError("Truncated native integrity label")
            sid = C.c_void_p.from_buffer(data).value
            if not sid or not self.security.IsValidSid(sid):
                raise ValueError("Invalid native integrity SID")
            count = self.security.GetSidSubAuthorityCount(sid)[0]
            if not count:
                raise ValueError("Native integrity SID lacks an authority")
            integrity = self.security.GetSidSubAuthority(sid, count - 1)[0]
            groups = self.token_buffer(token, 11)  # TokenRestrictedSids / TOKEN_GROUPS.
            if len(groups) < C.sizeof(W.DWORD):
                raise ValueError("Truncated native restricted SID groups")
            return {
                "integrityRid": int(integrity),
                "isAppContainer": bool(self.token_number(token, 29)),
                "hasRestrictions": self.token_filtered(token),
                "restrictedSids": W.DWORD.from_buffer(groups).value > 0,
            }

    def alive(self, process):
        state = self.kernel.WaitForSingleObject(process, 0)
        if state not in (0, 258):
            raise C.WinError(C.get_last_error())
        return state == 258

    def inspect(self, pid):
        if type(pid) is not int or not 0 < pid <= 0xFFFFFFFF:
            raise ValueError("Expected a positive native DWORD process ID")
        # Only query/liveness rights: no debug privilege, memory access or mutation.
        handle = self.kernel.OpenProcess(0x1000 | 0x100000, False, pid)
        if not handle and C.get_last_error() == 87:
            raise ProcessLookupError(pid)
        with self.handle(handle) as process:
            if not self.alive(process):
                raise ProcessLookupError(pid)
            try:
                return self.inspect_handle(process, pid)
            except OSError as error:
                # A helper can exit between a query and the final liveness read.
                # Do not turn a live-process permission/API failure into a skipped record.
                if not self.alive(process):
                    raise ProcessLookupError(pid) from error
                raise

    def inspect_handle(self, process, pid):
        length = W.DWORD(32768)
        image = C.create_unicode_buffer(length.value)
        if not self.kernel.QueryFullProcessImageNameW(process, 0, image, C.byref(length)):
            raise C.WinError(C.get_last_error())
        creation, exit_time, kernel, user = (W.FILETIME() for _ in range(4))
        if not self.kernel.GetProcessTimes(
            process, C.byref(creation), C.byref(exit_time), C.byref(kernel), C.byref(user)
        ):
            raise C.WinError(C.get_last_error())
        in_job = W.BOOL()
        if not self.kernel.IsProcessInJob(process, None, C.byref(in_job)):
            raise C.WinError(C.get_last_error())
        value = {
            "pid": pid,
            "image": str(Path(image.value).resolve(strict=True)),
            "creationTicks": (creation.dwHighDateTime << 32) | creation.dwLowDateTime,
            "inJob": bool(in_job.value),
            "token": self.token(process),
        }
        if not self.alive(process):
            raise ProcessLookupError(pid)
        return value
