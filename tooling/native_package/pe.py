"""Stage the import-table DLL closure of trusted Windows x64 build outputs."""

import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile

from check_checkout_paths import reason
from native_package.elf import digest

# Native host ABI for the launcher, runtime and Qt platform plugin. Windows
# graphics, input and shell libraries come from System32. VC redistributables
# still require an explicit app-local provider.
HOST_DLLS = frozenset(
    {
        "advapi32.dll",
        "bcrypt.dll",
        "bcryptprimitives.dll",
        "combase.dll",
        "comdlg32.dll",
        "crypt32.dll",
        "d3d9.dll",
        "d3d11.dll",
        "d3d12.dll",
        "d3dcompiler_47.dll",
        "dbghelp.dll",
        "dcomp.dll",
        "dnsapi.dll",
        "dwmapi.dll",
        "dwrite.dll",
        "dxgi.dll",
        "gdi32.dll",
        "imm32.dll",
        "iphlpapi.dll",
        "kernel32.dll",
        "mpr.dll",
        "ncrypt.dll",
        "netapi32.dll",
        "ntdll.dll",
        "ole32.dll",
        "oleaut32.dll",
        "opengl32.dll",
        "psapi.dll",
        "secur32.dll",
        "setupapi.dll",
        "shcore.dll",
        "shell32.dll",
        "shlwapi.dll",
        "uiautomationcore.dll",
        "ucrtbase.dll",
        "user32.dll",
        "userenv.dll",
        "uxtheme.dll",
        "version.dll",
        "winmm.dll",
        "winspool.drv",
        "ws2_32.dll",
        "wtsapi32.dll",
    }
)
DLL = re.compile(r"[a-zA-Z0-9_][a-zA-Z0-9_.-]*\.dll", re.I)
UCRT_CONTRACT = re.compile(r"api-ms-win-crt-[a-z0-9-]+-l\d+-\d+-\d+\.dll")

HOST_API_SETS = {"api-ms-win-core-synch-l1-2-0.dll": "kernelbase.dll"}


def api_set_host(name):
    """Return the expected system host for a supported virtual import."""
    if UCRT_CONTRACT.fullmatch(name):
        return "ucrtbase.dll"
    return HOST_API_SETS.get(name)


def physical_file(path):
    attributes = path.lstat()
    if not stat.S_ISREG(attributes.st_mode) or (
        getattr(attributes, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
    ):
        raise ValueError(f"Expected a physical Windows binary: {path}")


def image_dependencies(dumpbin, path):
    """Read import tables and export forwarders from a native x64 PE report."""
    result = subprocess.run(
        [dumpbin, "/NOLOGO", "/HEADERS", "/DEPENDENTS", "/EXPORTS", str(path)],
        capture_output=True,
        check=True,
        timeout=30,
    )
    # Only the ASCII table markers are interpreted. A Unicode path in the
    # dump banner may be represented using the runner's console code page.
    report = result.stdout.decode("ascii", errors="replace")
    if (
        not re.search(r"^\s*8664 machine \(x64\)\s*$", report, re.M)
        or not re.search(r"^\s*20B magic # \(PE32\+\)\s*$", report, re.M | re.I)
        or not re.search(r"^\s*File Type: (?:EXECUTABLE IMAGE|DLL)\s*$", report, re.M)
        or not re.search(r"^\s*Summary\s*$", report, re.M)
    ):
        raise ValueError(f"Expected a native x64 PE image: {path}")

    names = set()
    lines = iter(report.splitlines())
    for line in lines:
        heading = line.strip().lower()
        if heading not in (
            "image has the following dependencies:",
            "image has the following delay load dependencies:",
        ):
            continue
        first = next(lines, "").strip()
        while not first:
            first = next(lines, None)
            if first is None:
                raise ValueError(f"Empty PE dependency table: {path}")
            first = first.strip()
        for name in (first, *take_table(lines)):
            normalized = name.lower()
            if not DLL.fullmatch(name) and normalized not in HOST_DLLS:
                raise ValueError(f"Invalid PE import name in {path.name}: {name!r}")
            names.add(normalized)
    forwarders = set()
    for line in report.splitlines():
        if "(forwarded to " not in line:
            continue
        match = re.search(r"\(forwarded to ([a-z0-9_][a-z0-9_.-]*)\.[^\s.()]+\)\s*$", line, re.I)
        if not match:
            raise ValueError(f"Unrecognized PE export forwarder in {path.name}: {line.strip()!r}")
        module = match[1].lower()
        name = module if module.endswith(".dll") or module in HOST_DLLS else module + ".dll"
        if reason(name):
            raise ValueError(f"Invalid PE export module in {path.name}: {name!r}")
        forwarders.add(name)
    return {"imports": sorted(names), "forwarders": sorted(forwarders)}


def take_table(lines):
    for line in lines:
        name = line.strip()
        if not name:
            break
        yield name


def system_directory():
    import ctypes

    buffer = ctypes.create_unicode_buffer(32768)
    size = ctypes.windll.kernel32.GetSystemDirectoryW(buffer, len(buffer))
    if not 0 < size < len(buffer):
        raise ctypes.WinError()
    return Path(buffer.value)


def verify_api_set(name, directory, expected_host):
    """Resolve the virtual import with the loader and verify its System32 path."""
    import ctypes
    from _ctypes import FreeLibrary

    try:
        library = ctypes.WinDLL(name, winmode=0x800)  # LOAD_LIBRARY_SEARCH_SYSTEM32
    except OSError as error:
        raise ValueError(f"Unresolved Windows API-set contract: {name}") from error
    try:
        module_path = ctypes.windll.kernel32.GetModuleFileNameW
        module_path.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32]
        module_path.restype = ctypes.c_uint32
        buffer = ctypes.create_unicode_buffer(32768)
        size = module_path(library._handle, buffer, len(buffer))
        if not 0 < size < len(buffer) or Path(buffer.value) != directory / expected_host:
            raise ValueError(f"Unexpected Windows API-set provider for {name}")
    finally:
        FreeLibrary(library._handle)


def stage(output, images, providers, *, plugins=()):
    """Stage executable roots and explicitly placed dynamic plugin roots.

    Each plugin is a (relative path, source) pair. Its linked app-local DLLs
    live beside the executable, just like the executable's other providers.
    """
    if os.name != "nt":
        raise RuntimeError("PE closure staging requires native Windows")
    dumpbin = shutil.which("dumpbin")
    if not dumpbin:
        raise RuntimeError("Missing native packaging tool: dumpbin")
    if output.exists() or output.is_symlink():
        raise ValueError("PE output directory must be fresh")

    os_directory = system_directory()
    supplied = {}
    pending = {}
    for source in providers:
        name = source.name.lower()
        if (
            not DLL.fullmatch(name)
            or reason(name)
            or name in HOST_DLLS
            or api_set_host(name) is not None
            or name in supplied
        ):
            raise ValueError(f"Invalid or duplicate DLL provider: {source}")
        physical_file(source)
        supplied[name] = (source.name, source)
    for source in images:
        name = source.name.lower()
        if source.suffix.lower() != ".exe" or reason(name) or name in pending:
            raise ValueError(f"Invalid or duplicate executable input: {source}")
        physical_file(source)
        pending[name] = (source.name, source)
    if not pending:
        raise ValueError("PE closure requires at least one executable")

    for relative, source in plugins:
        name = source.name.lower()
        if (
            "/" not in relative
            or reason(relative)
            or relative.rsplit("/", 1)[1].lower() != name
            or not DLL.fullmatch(name)
            or name in HOST_DLLS
            or api_set_host(name) is not None
            or name in supplied
            or name in pending
        ):
            raise ValueError(f"Invalid or duplicate PE plugin: {relative!r}")
        physical_file(source)
        pending[name] = (relative, source)

    planned = {}
    host = set()
    missing = set()
    while pending:
        key, (relative, source) = pending.popitem()
        fingerprint = digest(source)
        dependencies = image_dependencies(dumpbin, source)
        planned[key] = (relative, source, fingerprint, dependencies)
        for name in sorted(set(dependencies["imports"]) | set(dependencies["forwarders"])):
            if name in host:
                continue
            if name in HOST_DLLS:
                physical_file(os_directory / name)
                host.add(name)
            elif (expected_host := api_set_host(name)) is not None:
                verify_api_set(name, os_directory, expected_host)
                host.add(name)
            elif name in supplied:
                if name not in planned:
                    pending[name] = supplied[name]
            else:
                missing.add((name, source.name))

    if missing:
        raise ValueError(
            "\n".join(
                f"No explicit DLL provider for {name}, required by {consumer}"
                for name, consumer in sorted(missing)
            )
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".pe-stage-", dir=output.parent) as temporary:
        payload = Path(temporary) / "bin"
        payload.mkdir()
        for relative, source, fingerprint, _ in planned.values():
            target = payload / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            if digest(target) != fingerprint:
                raise ValueError(f"PE input changed during staging: {source}")
        payload.rename(output)

    return {
        "objects": {
            relative: {"sha256": fingerprint, **dependencies}
            for relative, _, fingerprint, dependencies in planned.values()
        },
        "hostDlls": sorted(host),
    }
