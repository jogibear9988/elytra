#!/usr/bin/env python3
"""Compare encrypted HTTP clients and verify no HTTP requests pass failed TLS verification."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import socket
import socketserver
import ssl
import subprocess
import threading


class WirePeer(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        try:
            initial = self.request.recv(5, socket.MSG_PEEK)
            assert initial[:1] == b"\x16", initial
            self.server.encrypted = True
            if self.server.mode == "plain":
                self.request.recv(65536)
                self.request.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n")
                while self.request.recv(65536):
                    pass
                return
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(
                str(self.server.certificates / "server.pem"),
                str(self.server.certificates / "server-key.pem"),
            )
            secure = context.wrap_socket(self.request, server_side=True)
            with secure:
                headers = b""
                while not headers.endswith(b"\r\n\r\n"):
                    data = secure.recv(1)
                    assert data and len(headers) < 65536
                    headers += data
                assert headers.startswith(b"GET / HTTP/1.1\r\n"), headers
                self.server.request_seen = True
                if self.server.mode == "truncated":
                    secure.sendall(
                        b"HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\nshort"
                    )
                else:
                    secure.sendall(
                        b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n3\r\none\r\n3\r\ntwo\r\n0\r\nX-End: done\r\n\r\n"
                    )
                try:
                    secure.unwrap().close()
                except (ssl.SSLEOFError, ConnectionResetError):
                    if self.server.mode != "truncated":
                        raise
        except (ConnectionResetError, BrokenPipeError):
            if self.server.mode != "plain":
                self.server.failure = "Unexpected peer reset"
        except Exception as error:
            self.server.failure = repr(error)
        finally:
            self.server.done.set()


class WireServer(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-only", action="store_true")
    parser.add_argument("--host", required=True)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for HTTPS differentials")
    root = Path(__file__).resolve().parents[2] / "mite/tests/https"
    certificates = root.parent / "tls/fixtures"
    policy = subprocess.run(
        [args.mite, str(root / "policy.mjs")], capture_output=True, text=True, timeout=10
    )
    assert policy.returncode == 0 and not policy.stderr and policy.stdout == "HTTPS policy 15\n", (
        policy
    )
    defaults = [
        subprocess.run(
            [exe, str(root / "defaults.mjs")], capture_output=True, text=True, timeout=10
        )
        for exe in ([args.mite] if args.native_only else [args.node, args.mite])
    ]
    assert all(
        result.returncode == 0 and not result.stderr and result.stdout == "defaults 4 true true\n"
        for result in defaults
    ), defaults
    print("PASS HTTPS protocol/default/class identity and unavailable policy", flush=True)
    for mode in ["plain", "chunked", "truncated"]:
        for exe in [args.mite] if args.native_only else [args.node, args.mite]:
            with WireServer(("127.0.0.1", 0), WirePeer) as server:
                server.mode = mode
                server.certificates = certificates
                server.encrypted = False
                server.request_seen = False
                server.failure = None
                server.done = threading.Event()
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                try:
                    client = subprocess.run(
                        [
                            exe,
                            str(root / "raw-client.mjs"),
                            str(server.server_address[1]),
                            str(certificates),
                            mode,
                        ],
                        capture_output=True,
                        text=True,
                        timeout=10,
                    )
                    assert client.returncode == 0 and not client.stderr, client
                    # Preserve actual provider codes: Node wraps this negotiation
                    # failure as EPROTO, Mite reports the OpenSSL reason directly.
                    expected = (
                        f"raw plain {'EPROTO' if exe == args.node else 'ERR_SSL_WRONG_VERSION_NUMBER'} true true\n"
                        if mode == "plain"
                        else f"raw {mode} true true\n"
                    )
                    assert client.stdout == expected, client
                    assert server.done.wait(5) and server.encrypted and server.failure is None, (
                        server.failure,
                        server.encrypted,
                    )
                    assert server.request_seen == (mode != "plain"), server.request_seen
                finally:
                    server.shutdown()
                    thread.join(timeout=2)
        print("PASS independent TLS wire/framing peer:", mode, flush=True)
    for exe in [args.mite] if args.native_only else [args.node, args.mite]:
        peer = subprocess.Popen(
            [args.node, str(root / "peer.mjs"), str(certificates), "mismatch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        watchdog = threading.Timer(15, peer.kill)
        watchdog.start()
        try:
            port = peer.stdout.readline().strip()
            assert port.isdigit(), port
            failed = subprocess.run(
                [exe, str(root / "fatal.mjs"), port, str(certificates)],
                capture_output=True,
                text=True,
                timeout=10,
            )
            out, err = peer.communicate("done\n", timeout=3)
            assert failed.returncode == 1 and not failed.stdout and failed.stderr, (exe, failed)
            assert (
                peer.returncode == 0 and not err and json.loads(out) == {"requests": 0, "bytes": 0}
            ), (out, err)
        finally:
            watchdog.cancel()
            if peer.poll() is None:
                peer.kill()
                peer.communicate()
    print(
        "PASS fatal HTTPS certificate error still exits without later close callbacks", flush=True
    )
    for mode in [
        "get",
        "ip",
        "ipv6",
        "dns",
        "custom-host",
        "no-alpn",
        "permissive",
        "late-abort",
        "gc",
        "v12",
        "url",
        "post",
        "head",
        "empty",
        "chunked",
        "large",
        "cancel",
        "pre",
        "socket-abort",
        "mismatch",
        "expired",
        "untrusted",
    ]:
        results = []
        candidates = [args.host if mode == "gc" else args.mite]
        if not args.native_only:
            candidates.insert(0, args.node)
        for exe in candidates:
            peer = subprocess.Popen(
                [args.node, str(root / "peer.mjs"), str(certificates), mode],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            watchdog = threading.Timer(20, peer.kill)
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
                        str(certificates),
                        mode,
                    ],
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                stdout, stderr = peer.communicate("done\n", timeout=3)
                assert peer.returncode == 0 and not stderr, (stdout, stderr, peer.returncode)
                report = json.loads(stdout)
                assert client.returncode == 0 and client.stderr == (
                    "HOST_SHUTDOWN\n" if exe == args.host else ""
                ), (mode, exe, client, report)
                assert report == {
                    "requests": 0
                    if mode in ["mismatch", "expired", "untrusted", "pre", "socket-abort"]
                    else 1,
                    "bytes": 11 if mode == "post" else 0,
                }, (mode, exe, report, client)
                results.append(client.stdout)
            finally:
                watchdog.cancel()
                if peer.poll() is None:
                    peer.kill()
                    peer.communicate()
        expected = (
            f"early abort {mode} true true true\n"
            if mode in ["pre", "socket-abort"]
            else f"reject {mode} true true true\n"
            if mode in ["mismatch", "expired", "untrusted"]
            else "cancel true true true true\n"
            if mode == "cancel"
            else f"HTTPS {mode} true true true\n"
        )
        assert results == [expected] * len(candidates), (mode, results)
        print("PASS actual HTTPS:", mode, flush=True)


if __name__ == "__main__":
    main()
