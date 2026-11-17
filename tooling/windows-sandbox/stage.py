#!/usr/bin/env python3
"""Publish a physical, sealed Windows helper tree from actual target/resource inputs.

The seal is published last. Partial work remains explicitly journaled and cannot
be admitted by the native launcher. No source checkout or unowned output is removed.
"""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat

SEAL = ".firefly-windows-runtime-v1"
JOURNAL = ".firefly-windows-stage-v1.json"
MAX_FILES = 16384
MAX_FILE = 512 * 1024 * 1024
MAX_TOTAL = 2 * 1024 * 1024 * 1024


def physical(path, file=False):
    for item in [path, *path.parents]:
        if not item.exists() and not item.is_symlink():
            continue
        info = item.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("Refuse reparse/symlink helper paths")
    if file:
        info = path.stat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise ValueError("Helper files must have one physical link")


def digest(path):
    physical(path, file=True)
    if path.stat().st_size > MAX_FILE:
        raise ValueError("Helper file exceeds byte budget")
    result = hashlib.sha256()
    with path.open("rb") as source:
        while data := source.read(65536):
            result.update(data)
    return result.hexdigest()


class Creator:
    """New Windows objects explicitly use TokenUser, including elevated build hosts."""

    def __init__(self):
        self.security = None
        self.pins = {}
        if os.name != "nt":
            return
        from ctypes import wintypes as W

        class Attributes(ctypes.Structure):
            _fields_ = [("length", W.DWORD), ("descriptor", ctypes.c_void_p), ("inherit", W.BOOL)]

        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.advapi = ctypes.WinDLL("advapi32", use_last_error=True)
        self.kernel.GetCurrentProcess.restype = W.HANDLE
        self.kernel.CloseHandle.argtypes = [W.HANDLE]
        self.advapi.OpenProcessToken.argtypes = [W.HANDLE, W.DWORD, ctypes.POINTER(W.HANDLE)]
        self.advapi.GetTokenInformation.argtypes = [
            W.HANDLE,
            ctypes.c_int,
            ctypes.c_void_p,
            W.DWORD,
            ctypes.POINTER(W.DWORD),
        ]
        self.advapi.InitializeSecurityDescriptor.argtypes = [ctypes.c_void_p, W.DWORD]
        self.advapi.SetSecurityDescriptorOwner.argtypes = [ctypes.c_void_p, ctypes.c_void_p, W.BOOL]
        token = W.HANDLE()
        if not self.advapi.OpenProcessToken(
            self.kernel.GetCurrentProcess(), 8, ctypes.byref(token)
        ):
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            size = W.DWORD()
            self.advapi.GetTokenInformation(token, 1, None, 0, ctypes.byref(size))
            self.user = ctypes.create_string_buffer(size.value)
            if not self.advapi.GetTokenInformation(token, 1, self.user, size, ctypes.byref(size)):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            self.kernel.CloseHandle(token)
        sid = ctypes.c_void_p.from_buffer(self.user).value
        self.descriptor = ctypes.create_string_buffer(64)
        if not self.advapi.InitializeSecurityDescriptor(
            self.descriptor, 1
        ) or not self.advapi.SetSecurityDescriptorOwner(self.descriptor, sid, False):
            raise ctypes.WinError(ctypes.get_last_error())
        self.security = Attributes(
            ctypes.sizeof(Attributes), ctypes.addressof(self.descriptor), False
        )
        self.kernel.CreateDirectoryW.argtypes = [W.LPCWSTR, ctypes.POINTER(Attributes)]
        self.kernel.CreateFileW.argtypes = [
            W.LPCWSTR,
            W.DWORD,
            W.DWORD,
            ctypes.POINTER(Attributes),
            W.DWORD,
            W.DWORD,
            W.HANDLE,
        ]
        self.kernel.CreateFileW.restype = W.HANDLE

        class Information(ctypes.Structure):
            _fields_ = [
                ("attributes", W.DWORD),
                ("created", W.FILETIME),
                ("accessed", W.FILETIME),
                ("written", W.FILETIME),
                ("volume", W.DWORD),
                ("size_high", W.DWORD),
                ("size_low", W.DWORD),
                ("links", W.DWORD),
                ("index_high", W.DWORD),
                ("index_low", W.DWORD),
            ]

        self.Information = Information
        self.kernel.GetFileInformationByHandle.argtypes = [W.HANDLE, ctypes.POINTER(Information)]
        self.kernel.GetFinalPathNameByHandleW.argtypes = [W.HANDLE, W.LPWSTR, W.DWORD, W.DWORD]
        self.kernel.GetFinalPathNameByHandleW.restype = W.DWORD
        self.kernel.SetFileInformationByHandle.argtypes = [
            W.HANDLE,
            ctypes.c_int,
            ctypes.c_void_p,
            W.DWORD,
        ]

        class Rename(ctypes.Structure):
            _fields_ = [
                ("replace", W.BOOL),
                ("root", W.HANDLE),
                ("length", W.DWORD),
                ("name", W.WCHAR * 1),
            ]

        self.Rename = Rename

    def close(self):
        for handle in reversed(list(self.pins.values())):
            self.kernel.CloseHandle(handle)
        self.pins.clear()

    def pin(self, path):
        if self.security is None or path in self.pins:
            return
        removable = hasattr(self, "root") and (path == self.root or self.root in path.parents)
        access = 0x80 | (0x10000 if removable else 0)
        handle = self.kernel.CreateFileW(str(path), access, 3, None, 3, 0x02200000, None)
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            info = self.Information()
            actual = ctypes.create_unicode_buffer(32768)
            count = self.kernel.GetFinalPathNameByHandleW(handle, actual, len(actual), 0)
            if (
                not self.kernel.GetFileInformationByHandle(handle, ctypes.byref(info))
                or not info.attributes & 0x10
                or info.attributes & 0x400
                or not count
                or count >= len(actual)
                or actual.value.casefold() != ("\\\\?\\" + str(path)).casefold()
            ):
                raise ValueError("Destination directory identity/reparse mismatch")
        except BaseException:
            self.kernel.CloseHandle(handle)
            raise
        self.pins[path] = handle

    def directory(self, path):
        if path.parent != path:
            self.directory(path.parent)
        if not path.exists():
            if self.security is None:
                path.mkdir()
            elif not self.kernel.CreateDirectoryW(str(path), ctypes.byref(self.security)):
                raise ctypes.WinError(ctypes.get_last_error())
        physical(path)
        self.pin(path)

    def file(self, path):
        self.directory(path.parent)
        if self.security is None:
            return path.open("x+b", buffering=0)
        import msvcrt

        # Keep this original CREATE_NEW handle through every write and rename.
        # No empty-file-close/path-reopen window can truncate a foreign alias.
        handle = self.kernel.CreateFileW(
            str(path), 0xC0010000, 1, ctypes.byref(self.security), 1, 0x00200080, None
        )
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            fd = msvcrt.open_osfhandle(handle, os.O_RDWR | os.O_BINARY)
        except BaseException:
            self.kernel.CloseHandle(handle)
            raise
        try:
            return os.fdopen(fd, "w+b", buffering=0)
        except BaseException:
            os.close(fd)
            raise

    def remove_file(self, path, expected):
        if self.security is None:
            if digest(path) != expected:
                raise ValueError("Changed retired helper file")
            path.unlink()
            return
        import msvcrt

        handle = self.kernel.CreateFileW(str(path), 0x80010000, 1, None, 3, 0x00200000, None)
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            info = self.Information()
            if (
                not self.kernel.GetFileInformationByHandle(handle, ctypes.byref(info))
                or info.attributes & 0x410
                or info.links != 1
            ):
                raise ValueError("Retired helper file identity changed")
            fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
        except BaseException:
            self.kernel.CloseHandle(handle)
            raise
        with os.fdopen(fd, "rb", buffering=0) as source:
            result = hashlib.sha256()
            while data := source.read(65536):
                result.update(data)
            if result.hexdigest() != expected:
                raise ValueError("Changed retired helper file")
            deletion = ctypes.c_ubyte(1)
            if not self.kernel.SetFileInformationByHandle(handle, 4, ctypes.byref(deletion), 1):
                raise ctypes.WinError(ctypes.get_last_error())

    def remove_directory(self, path):
        if self.security is None:
            path.rmdir()
            return
        handle = self.pins[path]
        deletion = ctypes.c_ubyte(1)
        if not self.kernel.SetFileInformationByHandle(handle, 4, ctypes.byref(deletion), 1):
            raise ctypes.WinError(ctypes.get_last_error())
        self.kernel.CloseHandle(self.pins.pop(path))

    def publish(self, output, temporary, destination):
        # FILE_RENAME_INFO accepts an absolute destination and operates on the
        # original DELETE-capable file handle, even while name replacement is
        # denied to other callers. See Microsoft's FILE_RENAME_INFO contract.
        output.flush()
        os.fsync(output.fileno())
        if self.security is None:
            os.replace(temporary, destination)
            return
        import msvcrt

        encoded = str(destination).encode("utf-16-le")
        buffer = ctypes.create_string_buffer(self.Rename.name.offset + len(encoded))
        rename = self.Rename.from_buffer(buffer)
        rename.replace = True
        rename.root = None
        rename.length = len(encoded)
        ctypes.memmove(ctypes.addressof(buffer) + self.Rename.name.offset, encoded, len(encoded))
        if not self.kernel.SetFileInformationByHandle(
            msvcrt.get_osfhandle(output.fileno()), 3, buffer, len(buffer)
        ):
            raise ctypes.WinError(ctypes.get_last_error())


def entries(root):
    result = {}
    for path in root.rglob("*"):
        physical(path)
        if path.is_file() and path.name != ".DS_Store":
            result[path.relative_to(root).as_posix()] = digest(path)
    return result


def valid_name(name):
    parts = name.split("/")
    return (
        (
            len(parts) == 2
            and parts[0] == "bin"
            or len(parts) >= 3
            and parts[:2] == ["share", "Lagom"]
        )
        and all(part and part not in (".", "..", ".DS_Store") for part in parts)
        and not any(char in name for char in '\\:*?"<>|')
        and not any(ord(char) < 32 for char in name)
    )


def parse_seal(path):
    physical(path, file=True)
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or not re.fullmatch(r"FWR1 [0-9a-f]{40}", lines[0]):
        raise ValueError("Invalid helper ownership seal")
    result = {}
    for line in lines[1:]:
        match = re.fullmatch(r"([0-9a-f]{64}) ([0-9]+) (.+)", line)
        if not match:
            raise ValueError("Malformed helper inventory record")
        name = match[3]
        if name in result or not valid_name(name):
            raise ValueError("Invalid helper inventory path")
        result[name] = match[1]
    return result


def _stage(root, targets, resources, revision, creator):
    if not re.fullmatch(r"[0-9a-f]{40}", revision) or not root.is_absolute():
        raise ValueError("Invalid helper stage revision/root")
    physical(root)
    physical(resources)
    plan = {}
    for line in targets.read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        if "|" in line:
            name, source = line.split("|", 1)
        else:
            source = line
            name = Path(source).name
        if Path(name).name != name or any(ord(char) < 32 for char in name) or name in (".", ".."):
            raise ValueError("Invalid helper target basename")
        source = Path(source)
        expected = digest(source)
        key = "bin/" + name
        if not valid_name(key):
            raise ValueError("Invalid helper target path")
        if key in plan and plan[key][1] != expected:
            raise ValueError("Conflicting target DLL basename")
        plan[key] = (source, expected)
    if "bin/WebContent.exe" not in plan:
        raise ValueError("Missing real WebContent target")
    for source in resources.rglob("*"):
        physical(source)
        if source.is_file() and source.name != ".DS_Store":
            plan["share/Lagom/" + source.relative_to(resources).as_posix()] = (
                source,
                digest(source),
            )
    if (
        len(plan) > MAX_FILES
        or not all(valid_name(name) for name in plan)
        or sum(source.stat().st_size for source, _ in plan.values()) > MAX_TOTAL
        or not any(name.startswith("share/Lagom/") for name in plan)
    ):
        raise ValueError("Helper resource inventory missing/oversized")
    if len({name.casefold() for name in plan}) != len(plan):
        raise ValueError("Case-colliding helper files")
    new = {name: expected for name, (_, expected) in plan.items()}
    directories = {"", "bin", "share", "share/Lagom"}
    for name in plan:
        if len(name.encode("utf-8")) > 32768:
            raise ValueError("Helper inventory path exceeds native bound")
        parent = Path(name).parent
        while parent != Path("."):
            directories.add(parent.as_posix())
            parent = parent.parent
    if len(plan) + len(directories) + 1 > MAX_FILES:
        raise ValueError("Helper inventory exceeds native entry budget")
    content = (
        "FWR1 "
        + revision
        + "\n"
        + "".join(
            f"{expected} {source.stat().st_size} {name}\n"
            for name, (source, expected) in sorted(plan.items())
        )
    )
    if len(content.encode("utf-8")) > 4 * 1024 * 1024:
        raise ValueError("Helper inventory seal exceeds native bound")
    old = {}
    if root.exists():
        recovering = (root / JOURNAL).exists()
        if recovering:
            physical(root / JOURNAL, file=True)
            journal = json.loads((root / JOURNAL).read_text())
            if journal.get("new") != new or journal.get("revision") != revision:
                raise ValueError("Unfinished helper stage requires its exact original plan")
            old = journal["old"]
            if not isinstance(old, dict) or not all(
                valid_name(name) and re.fullmatch(r"[0-9a-f]{64}", value)
                for name, value in old.items()
            ):
                raise ValueError("Malformed helper stage journal")
        elif (root / SEAL).exists():
            old = parse_seal(root / SEAL)
        else:
            raise ValueError("Refuse unowned helper stage")
        known_directories = set(directories)
        for name in old:
            parent = Path(name).parent
            while parent != Path("."):
                known_directories.add(parent.as_posix())
                parent = parent.parent
        for name in sorted(known_directories, key=lambda value: len(value.split("/"))):
            path = root / name
            if path.exists():
                creator.directory(path)
        for path in root.rglob("*"):
            physical(path)
            if path.is_dir() and path.relative_to(root).as_posix() not in known_directories:
                raise ValueError("Unknown helper output directory")
        actual_entries = entries(root)
        if not recovering and any(
            actual_entries.get(name) != expected for name, expected in old.items()
        ):
            raise ValueError("Missing/changed sealed helper file")
        for name, actual in actual_entries.items():
            if name in (SEAL, JOURNAL):
                continue
            if actual not in ((old.get(name), new.get(name)) if recovering else (old.get(name),)):
                raise ValueError("Unknown or modified helper output")
    creator.directory(root)
    journal = root / JOURNAL
    if not journal.exists():
        with creator.file(journal) as output:
            output.write(json.dumps({"revision": revision, "old": old, "new": new}).encode("utf-8"))
            os.fsync(output.fileno())
    for name, (source, expected) in sorted(plan.items()):
        destination = root / name
        creator.directory(destination.parent)
        if destination.exists() and digest(destination) == expected:
            continue
        temporary = destination.with_name(destination.name + ".firefly-stage-tmp")
        if temporary.exists() or temporary.is_symlink():
            raise ValueError("Unexpected helper temporary file")
        identity = None
        try:
            with creator.file(temporary) as output_file, source.open("rb") as input_file:
                info = os.fstat(output_file.fileno())
                identity = (info.st_dev, info.st_ino)
                shutil.copyfileobj(input_file, output_file, 65536)
                output_file.seek(0)
                copied = hashlib.sha256()
                while data := output_file.read(65536):
                    copied.update(data)
                if copied.hexdigest() != expected:
                    raise ValueError("Helper source changed during copy")
                creator.publish(output_file, temporary, destination)
        finally:
            if temporary.exists() or temporary.is_symlink():
                current = temporary.lstat()
                if identity == (current.st_dev, current.st_ino) and not stat.S_ISLNK(
                    current.st_mode
                ):
                    temporary.unlink()
                else:
                    raise ValueError("Helper temporary identity changed")
    for name, expected in old.items():
        if name not in new:
            path = root / name
            if name in actual_entries:
                creator.remove_file(path, expected)
    retired_directories = set()
    for name in old:
        parent = Path(name).parent
        while parent != Path("."):
            retired_directories.add(parent.as_posix())
            parent = parent.parent
    for name in sorted(
        retired_directories - directories, key=lambda value: len(value.split("/")), reverse=True
    ):
        path = root / name
        if path.exists():
            physical(path)
            # Preserve Finder metadata; an occupied obsolete directory keeps
            # the transaction unsealed instead of being recursively removed.
            creator.remove_directory(path)
    published = entries(root)
    published.pop(SEAL, None)
    published.pop(JOURNAL, None)
    if published != new:
        raise ValueError("Helper files changed before inventory publication")
    temporary = root / (SEAL + ".tmp")
    with creator.file(temporary) as output:
        output.write(content.encode("utf-8"))
        creator.publish(output, temporary, root / SEAL)
    journal.unlink()


def stage(root, targets, resources, revision):
    creator = Creator()
    creator.root = root
    try:
        _stage(root, targets, resources, revision, creator)
    finally:
        creator.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--targets", required=True, type=Path)
    parser.add_argument("--resources", required=True, type=Path)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()
    stage(args.root.absolute(), args.targets, args.resources, args.revision)
