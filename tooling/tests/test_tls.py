#!/usr/bin/env python3
"""Use independent Node TLS peers to compare real encrypted records and trust failures."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import socket
import subprocess
import threading


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for TLS differential tests")
    root = Path(__file__).resolve().parents[2] / "mite/tests/tls"
    policy = subprocess.run(
        [args.mite, str(root / "validation.mjs")], capture_output=True, text=True, timeout=10
    )
    assert policy.returncode == 0 and not policy.stderr and policy.stdout == "TLS policy 16\n", (
        policy
    )
    print("PASS explicit TLS option/mode policy", flush=True)
    for exe in [args.node, args.mite]:
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(5)
            outcome = []

            def stalled_peer():
                try:
                    peer, _ = listener.accept()
                    with peer:
                        peer.settimeout(5)
                        received = 0
                        while data := peer.recv(65536):
                            received += len(data)
                        outcome.append(received > 0)
                except Exception as error:
                    outcome.append(repr(error))

            thread = threading.Thread(target=stalled_peer, daemon=True)
            thread.start()
            pending = subprocess.run(
                [
                    exe,
                    str(root / "pending.mjs"),
                    str(listener.getsockname()[1]),
                    str(root / "fixtures"),
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            thread.join(timeout=6)
            assert (
                pending.returncode == 0
                and not pending.stderr
                and pending.stdout == "pending 0 true\n"
            ), pending
            assert outcome == [True], outcome
    print("PASS independent stalled handshake: ClientHello received then native EOF", flush=True)
    for version in ["TLSv1.2", "TLSv1.3"]:
        for mode in [
            "echo",
            "no-alpn",
            "array",
            "ip",
            "wildcard",
            "large",
            "early",
            "gc",
            "mismatch",
            "wildcard-mismatch",
            "expired",
            "selfsigned",
            "default-untrusted",
            "untrusted",
            "permissive",
            "permissive-host",
            "abort",
        ]:
            results = []
            for exe in [args.node, args.host if mode == "gc" else args.mite]:
                peer = subprocess.Popen(
                    [args.node, str(root / "peer.mjs"), str(root / "fixtures"), version, mode],
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                watchdog = threading.Timer(15, peer.kill)
                watchdog.start()
                try:
                    port = peer.stdout.readline().strip()
                    assert port.isdigit(), (port, peer.communicate(timeout=2))
                    client = subprocess.run(
                        [
                            exe,
                            *(["--expose-gc"] if mode == "gc" and exe == args.node else []),
                            str(root / "client.mjs"),
                            port,
                            str(root / "fixtures"),
                            mode,
                            version,
                        ],
                        capture_output=True,
                        text=True,
                        timeout=12,
                    )
                    stdout, stderr = peer.communicate(input="done\n", timeout=3)
                    assert peer.returncode == 0 and not stdout and not stderr, (
                        mode,
                        stdout,
                        stderr,
                        peer.returncode,
                    )
                    assert client.returncode == 0 and client.stderr == (
                        "HOST_SHUTDOWN\n" if exe == args.host else ""
                    ), (mode, exe, client)
                    results.append(client.stdout)
                finally:
                    watchdog.cancel()
                    if peer.poll() is None:
                        peer.kill()
                        peer.communicate()
            expected = (
                f"rejected {mode} true true true\n"
                if mode
                in [
                    "mismatch",
                    "wildcard-mismatch",
                    "untrusted",
                    "expired",
                    "selfsigned",
                    "default-untrusted",
                ]
                else (
                    "cancelled true true true\n"
                    if mode == "abort"
                    else f"TLS {mode} true true true true\n"
                )
            )
            assert results == [expected, expected], (mode, version, results)
            print("PASS actual TLS:", version, mode, flush=True)


if __name__ == "__main__":
    main()
