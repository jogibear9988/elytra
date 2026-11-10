#!/usr/bin/env python3
"""Check actual native parent/owned-helper origin configuration before first page script."""

import _support  # noqa: F401 - standalone shared-helper import path

import argparse
import json
import ctypes
import os
import queue
import sys
import tempfile
import threading
import time
from pathlib import Path
import re
import subprocess
from test_renderer import ROOT, environment, run, stop


def loaded_files(pid):
    """Inspect this owned child's OS mappings; never attach a debugger."""
    if sys.platform == "darwin":
        library = ctypes.CDLL("/usr/lib/libproc.dylib")
        library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        library.proc_pidpath.restype = ctypes.c_int
        buffer = ctypes.create_string_buffer(4096)
        assert library.proc_pidpath(pid, buffer, len(buffer)) > 0, "proc_pidpath failed"
        executable = Path(os.fsdecode(buffer.value)).resolve()
        output = subprocess.check_output(
            ["/usr/sbin/lsof", "-a", "-p", str(pid), "-Fn"], text=True, timeout=10
        )
        paths = {Path(line[1:]).resolve() for line in output.splitlines() if line.startswith("n/")}
    elif sys.platform == "linux":
        executable = (Path("/proc") / str(pid) / "exe").resolve(strict=True)
        paths = set()
        for line in (Path("/proc") / str(pid) / "maps").read_text().splitlines():
            fields = line.split(maxsplit=5)
            if len(fields) == 6 and fields[5].startswith("/"):
                paths.add(Path(fields[5]).resolve())
    else:
        raise AssertionError("Packaged helper mapping evidence is not implemented on this platform")
    return executable, paths


def package_layout(directory, platform):
    """Accept only the owned bundle/sidecar layouts, not SDK library fallbacks."""
    if platform == "darwin":
        return directory.parent / "Resources", directory.parent / "lib", None
    assert platform == "linux", "Packaged mapping evidence unsupported on this platform"
    assert directory.name == "bin" and directory.parent.name == "firefly-runtime", directory
    root = directory.parent.parent
    manifest = root / "BUILD.json"
    assert manifest.is_file() and not manifest.is_symlink(), manifest
    assert manifest.stat().st_size <= 4 * 1024 * 1024, manifest
    metadata = json.loads(manifest.read_text())
    assert metadata.get("platform") == "linux" and metadata.get("architecture") == "x64", metadata
    elf = metadata["elf"]
    assert elf["platform"] == "linux-x64", elf
    assert "firefly-runtime/bin/WebContent" in elf["binaries"], elf
    resources = directory.parent / "share/Lagom"
    icu_path = Path(metadata["icuData"]["helpers"]["path"])
    assert icu_path.parent == Path("firefly-runtime/share/Lagom/icu"), icu_path
    assert icu_path.name.startswith("icudt") and icu_path.suffix == ".dat", icu_path
    libraries = root / "lib"
    assert libraries.is_dir() and not libraries.is_symlink(), libraries
    assert libraries.resolve() == libraries, libraries
    inventory = elf["libraries"]
    assert isinstance(inventory, dict) and inventory, inventory
    allowed = set()
    for name in inventory:
        assert isinstance(name, str) and name not in ("", ".", "..") and Path(name).name == name, (
            name
        )
        library = (libraries / name).resolve(strict=True)
        assert library.is_relative_to(libraries), library
        allowed.add(library)
    return resources, libraries, allowed


def packaged_evidence(probe, directory, page):
    """Keep only our diagnostic child alive long enough to inspect real mappings."""
    with tempfile.TemporaryDirectory(prefix="firefly-package-probe-") as config:
        command = [
            str(probe),
            "--helper-directory",
            str(directory),
            "--protocol-policy",
            "--headless=manual",
            page,
        ]
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=environment(config),
            start_new_session=True,
        )
        lines = queue.Queue()
        stdout, stderr = [], []

        def drain(stream, output, notify):
            for line in stream:
                output.append(line)
                if notify:
                    lines.put(line)

        threads = [
            threading.Thread(target=drain, args=(process.stdout, stdout, True), daemon=True),
            threading.Thread(target=drain, args=(process.stderr, stderr, False), daemon=True),
        ]
        for thread in threads:
            thread.start()
        try:
            deadline = time.monotonic() + 20
            pid = None
            while time.monotonic() < deadline:
                try:
                    line = lines.get(timeout=min(0.25, max(0.01, deadline - time.monotonic())))
                except queue.Empty:
                    assert process.poll() is None, (process.returncode, stdout, stderr)
                    continue
                match = re.fullmatch(r"WebContent PID: (\d+)\n?", line)
                if match:
                    pid = int(match[1])
                    break
            assert pid and pid != process.pid, (stdout, stderr)
            parent = int(subprocess.check_output(["ps", "-p", str(pid), "-o", "ppid="], text=True))
            assert parent == process.pid, (pid, parent, process.pid)
            executable, files = loaded_files(pid)
            assert executable == (directory / "WebContent").resolve(), executable
            resources, library_root, allowed_libraries = package_layout(directory, sys.platform)
            icu = set((resources / "icu").glob("icudt*.dat"))
            assert icu and {path.resolve() for path in icu} & files, (
                "ICU not observed mapped/open",
                files,
            )
            libraries = {path for path in files if path.name.startswith("liblagom-")}
            assert any("web." in path.name for path in libraries), libraries
            assert libraries and all(path.is_relative_to(library_root) for path in libraries), (
                libraries
            )
            if allowed_libraries is not None:
                assert libraries <= allowed_libraries, (libraries, allowed_libraries)
            assert f"PROBE_HELPER_DIRECTORY={directory}\n" in stdout, stdout
            assert f"PROBE_RESOURCE_ROOT={resources}\n" in stdout, stdout
            assert any(f"FIREFLY_ORIGIN_POLICY_READY pid={pid} " in line for line in stderr), stderr
            print(
                "PASS owned packaged WebContent OS executable, relocated liblagom mappings and actual ICU mapping/open evidence"
            )
            print(
                f"OBSERVED packaged WebContent pid={pid} executable={executable} ICU={sorted(map(str, icu))}"
            )
        finally:
            stop(process)
            process.wait(timeout=10)
            for thread in threads:
                thread.join(timeout=5)


def invalid_directories(probe, directory, page):
    with tempfile.TemporaryDirectory(prefix="firefly-invalid-helper-") as temporary:
        root = Path(temporary).resolve()
        link = root / "linked"
        link.symlink_to(directory, target_is_directory=True)
        cases = [
            ["--helper-directory"],
            ["--helper-directory=relative"],
            ["--helper-directory", str(root / "missing")],
            ["--helper-directory", str(root)],
            ["--helper-directory", str(link)],
            ["--helper-directory", str(directory), "--helper-directory", str(directory)],
        ]
        for options in cases:
            result = run(probe, "--headless=text", page, *options)
            assert result.returncode != 0, result
            assert "WebContent PID:" not in result.stdout, result
            assert "FIREFLY_ORIGIN_POLICY_READY" not in result.stderr, result
    print(
        "PASS missing, duplicate, relative, incomplete and symlink helper directories reject before launch"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--runtime", action="store_true")
    parser.add_argument(
        "--helper-directory",
        type=Path,
        help="Physical relocated package helper directory; require OS mapping evidence",
    )
    args = parser.parse_args()
    directory = args.helper_directory.absolute() if args.helper_directory else None
    helper_options = ["--helper-directory", str(directory)] if directory else []
    if directory:
        assert args.helper.resolve() == (directory / "WebContent").resolve(), (
            "--helper must be selected package WebContent"
        )
    # The owned entry rejects malformed configuration before trying to connect to a parent.
    for options, error in [
        (["--firefly-origin-policy={}"], "Unknown origin configuration schema"),
        (
            ["--firefly-origin-policy=bad", "--firefly-origin-policy=bad"],
            "Duplicate origin configuration option",
        ),
    ]:
        result = subprocess.run(
            [str(args.helper.resolve()), *options], capture_output=True, text=True, timeout=10
        )
        assert result.returncode != 0 and error in result.stderr, result
    page = (ROOT / "firefly/tests/protocol/configuration.html").as_uri()
    cases = [([], False), (["--protocol-policy"], True)]
    if args.runtime and not directory:
        cases += [(["--protocol-runtime-helper", "--protocol-policy"], True)]
    pids = set()
    for options, configured in cases:
        result = run(args.probe.resolve(), *helper_options, *options, "--headless=text", page)
        match = re.search(r"^WebContent PID: (\d+)$", result.stdout, re.MULTILINE)
        assert result.returncode == 0 and match, result
        pid = int(match[1])
        assert pid > 0 and pid != result.pid, result
        pids.add(pid)
        native = re.findall(
            r"FIREFLY_ORIGIN_POLICY_READY pid=(\d+) configuration=(\{.*\})", result.stderr
        )
        if configured:
            matching = [json.loads(payload) for child, payload in native if int(child) == pid]
            assert matching == [
                {"schema": 1, "schemes": [{"scheme": "app", "standard": True, "secure": True}]}
            ], result
            assert "FIRST_SCRIPT origin=app://bundle other=app://other" in result.stdout, result
        else:
            assert not native and "FIRST_SCRIPT origin=null other=null" in result.stdout, result
        origins = re.search(r"ORIGINS=(\[.*\]) BRAND=true", result.stdout)
        assert origins, result
        custom = [
            "app://bundle:0",
            "app://bundle:443",
            "app://bundle",
            "app://xn--bcher-kva.example",
            "app://[::1]:9000",
        ]
        expected = custom if configured else ["null"] * 5
        expected += ["null", "null", "https://example.com", "http://example.com:8080"]
        expected += ["app://bundle" if configured else "null"]
        assert json.loads(origins[1]) == expected, result
        assert "NATIVE_ABSENT=true" in result.stdout, result
        assert "crash" not in result.stderr.lower(), result
    assert len(pids) == len(cases), pids
    print("PASS actual owned helper entry rejects invalid/duplicate policy before bootstrap")
    print(
        "PASS native parent-to-owned-renderer configuration before first script; default denial, exact child policy and absent page privileges"
    )
    print(
        "PASS actual Rust DOM origins: normalized tuples, port/host separation, IDNA, IPv6, relative URLs, builtins and getter brand check"
    )
    if args.runtime and not directory:
        print(
            "PASS actual production preload helper receives the same bounded native configuration"
        )

    if directory:
        invalid_directories(args.probe.resolve(), directory, page)
        packaged_evidence(args.probe.resolve(), directory, page)


if __name__ == "__main__":
    main()
