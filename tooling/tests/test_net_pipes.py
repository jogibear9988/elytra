#!/usr/bin/env python3
"""Compare genuine OS pipe streams in Node/Mite, including an independent native Python peer."""

import _support

import argparse
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import threading
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/net-pipes"


def endpoint(root, name):
    if os.name == "nt":
        return "\\\\.\\pipe\\elytra-" + uuid.uuid4().hex + "-" + name
    return str(root / name)


def windows_exchange(name, payload):
    # Actual Windows API pipe client, independent of either runtime/libuv implementation.
    import ctypes
    from ctypes import wintypes

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.LPVOID,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.HANDLE,
    ]
    kernel.CreateFileW.restype = wintypes.HANDLE
    for operation in [kernel.WriteFile, kernel.ReadFile]:
        operation.argtypes = [
            wintypes.HANDLE,
            wintypes.LPVOID,
            wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD),
            wintypes.LPVOID,
        ]
        operation.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    handle = kernel.CreateFileW(name, 0xC0000000, 0, None, 3, 0, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        sent = 0
        while sent < len(payload):
            buffer = ctypes.create_string_buffer(payload[sent : sent + 65536])
            count = wintypes.DWORD()
            if not kernel.WriteFile(handle, buffer, len(buffer) - 1, ctypes.byref(count), None):
                raise ctypes.WinError(ctypes.get_last_error())
            assert count.value > 0
            sent += count.value
        received = bytearray()
        while len(received) < len(payload):
            buffer = ctypes.create_string_buffer(65536)
            count = wintypes.DWORD()
            if not kernel.ReadFile(handle, buffer, 65536, ctypes.byref(count), None):
                raise ctypes.WinError(ctypes.get_last_error())
            assert count.value > 0
            received.extend(buffer.raw[: count.value])
        return bytes(received)
    finally:
        kernel.CloseHandle(handle)


def independent(binary, name):
    process = subprocess.Popen(
        [binary, str(FIXTURES / "external.mjs")],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env={**os.environ, "MITE_TEST_PIPE": name},
    )
    watchdog = threading.Timer(15, process.kill)
    watchdog.start()
    try:
        assert process.stdout.readline().strip() == "READY", "Native pipe listener did not start"
        payload = bytes(index % 251 for index in range(262173))
        if os.name == "nt":
            reply = windows_exchange(name, payload)
        else:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
                peer.settimeout(5)
                peer.connect(name)
                peer.sendall(payload)
                received = bytearray()
                while True:
                    data = peer.recv(65536)
                    if not data:
                        break
                    received.extend(data)
                reply = bytes(received)
        stdout, stderr = process.communicate(timeout=5)
        assert (
            reply == payload and process.returncode == 0 and stdout == "SERVED\n" and not stderr
        ), (process.returncode, stdout, stderr, len(reply))
    finally:
        watchdog.cancel()
        if process.poll() is None:
            process.kill()
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument(
        "--node-only",
        action="store_true",
        help="Development oracle only; never used by the Mite acceptance gate",
    )
    args = parser.parse_args()
    if not args.node or (not args.node_only and not args.mite):
        parser.error("Require Node and --mite, or explicit development --node-only")
    runtimes = [("Node", args.node)]
    if not args.node_only:
        runtimes.append(("Mite", str(args.mite.resolve())))
    # Keep Unix paths below sockaddr_un limits even on macOS's long default TMPDIR.
    with tempfile.TemporaryDirectory(
        prefix="mp-", dir=None if os.name == "nt" else "/tmp"
    ) as temporary:
        root = Path(temporary)
        fixtures = [
            "exchange-windows.mjs" if os.name == "nt" else "exchange.mjs",
            "errors.mjs",
            "close.mjs",
            "abort.mjs",
            "unref.mjs",
        ]
        if os.name != "nt":
            fixtures += ["stale.mjs", "oversize.mjs"]
        for fixture in fixtures:
            observations = []
            for label, binary in runtimes:
                fixture_env = {
                    **os.environ,
                    "MITE_TEST_PIPE": endpoint(root, label + "-Grüße " + fixture),
                }
                if fixture == "stale.mjs":
                    stale = endpoint(root, label + "-stale.sock")
                    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as orphan:
                        orphan.bind(stale)
                    fixture_env["MITE_STALE_PIPE"] = stale
                result = subprocess.run(
                    [binary, str(FIXTURES / fixture)],
                    cwd=root,
                    env=fixture_env,
                    text=True,
                    capture_output=True,
                    timeout=20,
                )
                assert result.returncode == 0 and not result.stderr, result
                observations.append(result.stdout)
            assert len(set(observations)) == 1, observations
            print(
                "PASS real pipe " + fixture + ": " + observations[0].strip().replace("\n", "; "),
                flush=True,
            )
        if not args.node_only:
            policy = subprocess.run(
                [str(args.mite.resolve()), str(FIXTURES / "policy.mjs")],
                cwd=root,
                env={**os.environ, "MITE_TEST_PIPE": endpoint(root, "Mite-policy")},
                capture_output=True,
                text=True,
                timeout=30,
            )
            assert (
                policy.returncode == 0 and not policy.stderr and policy.stdout.startswith("PASS ")
            ), policy
            print(policy.stdout.strip(), flush=True)
        for label, binary in runtimes:
            independent(binary, endpoint(root, label + "-external"))
            print("PASS " + label + " pipe listener against independent Python OS peer", flush=True)
    print(
        "PASS "
        + ("Node oracle only" if args.node_only else "Node/Mite real OS pipe differential")
        + "; host platform "
        + os.name
        + ", no unverified platform acceptance"
    )


if __name__ == "__main__":
    main()
