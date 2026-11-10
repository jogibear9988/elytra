#!/usr/bin/env python3
"""Keep owned libuv/OpenSSL declarations behind the reviewed WinSDK socket boundary."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
BOUNDARY = Path("mite/runtime/platform/UV.h")
SOCKET_BOUNDARY = Path("mite/runtime/platform/WindowsSockets.h")
# This separate native WinSDK-only sandbox fixture never includes AK/LibJS or
# runtime headers. Preserve its actual OS types rather than import runtime ones.
SDK_ONLY_SOCKET_USERS = {Path("firefly/native/sandbox/windows/Probe.cpp")}
OPENSSL = re.compile(r'^\s*#\s*include\s*[<"]openssl/', re.M)
WINSOCK = re.compile(r'^\s*#\s*include\s*[<"]winsock2?\.h', re.M | re.I)
OWNED_SOCKET_IMPORT = re.compile(r'^\s*#\s*include\s*"[^"\n]*(?:WindowsSockets|UV)\.h"', re.M)
DIRECT = re.compile(r'^\s*#\s*include\s*[<"]uv(?:\.h|/)', re.M)


def check(root=ROOT):
    rejected = []
    for directory in (root / "mite", root / "firefly"):
        for source in directory.rglob("*"):
            if source.suffix not in (".cpp", ".h", ".mm"):
                continue
            relative = source.relative_to(root)
            text = source.read_text()
            if relative != BOUNDARY and DIRECT.search(text):
                rejected.append(str(relative))
            if (
                relative != SOCKET_BOUNDARY
                and relative not in SDK_ONLY_SOCKET_USERS
                and WINSOCK.search(text)
            ):
                rejected.append(str(relative) + " (direct WinSDK socket include)")
            openssl = OPENSSL.search(text)
            if openssl and not OWNED_SOCKET_IMPORT.search(text[: openssl.start()]):
                rejected.append(str(relative) + " (OpenSSL before socket boundary)")
    if rejected:
        raise ValueError("Use the owned UV/WindowsSockets header boundary: " + ", ".join(rejected))
    print("PASS owned libuv/OpenSSL headers use the reviewed Windows socket boundary")


if __name__ == "__main__":
    check()
