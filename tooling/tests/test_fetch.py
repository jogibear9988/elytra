#!/usr/bin/env python3
"""Fetch acceptance with real Node and independent HTTP barriers; never launches a GUI."""

import argparse
import gzip
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import sys
import threading
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[2]


class Peer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.lock = threading.Lock()
        self.gates = {}
        self.seen = set()
        self.disconnected = set()
        self.errors = []
        self.compression_request = None

    def gate(self, key):
        with self.lock:
            return self.gates.setdefault(key, threading.Event())

    def handle_error(self, request, client_address):
        self.errors.append(str(sys.exc_info()[1]))


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def reply(self, body, status=200, headers=()):
        self.send_response(status)
        self.send_header("content-length", str(len(body)))
        self.send_header("connection", "close")
        self.send_header("x-peer", "independent")
        for key, value in headers:
            self.send_header(key, value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def read_body(self):
        if self.headers.get("transfer-encoding") == "chunked":
            chunks = []
            while True:
                size = int(self.rfile.readline().split(b";", 1)[0], 16)
                if not size:
                    assert self.rfile.readline() == b"\r\n"
                    return b"".join(chunks)
                chunks.append(self.rfile.read(size))
                assert self.rfile.read(2) == b"\r\n"
        return self.rfile.read(int(self.headers.get("content-length", "0")))

    def handle_request(self):
        self.connection.settimeout(10)
        self.close_connection = True
        url = urlsplit(self.path)
        self.server.seen.add(url.path)
        if url.path == "/early":
            self.reply(b"early", 413)
            return
        body = self.read_body()
        if url.path in {"/json", "/echo"}:
            result = {
                "path": self.path,
                "method": self.command,
                "body": body.decode(),
                "length": len(body),
                "type": self.headers.get("content-type"),
                "authorization": self.headers.get("authorization"),
                "cookie": self.headers.get("cookie"),
                "host": self.headers.get("host"),
            }
            self.reply(json.dumps(result).encode(), headers=[("content-type", "application/json")])
        elif url.path == "/error":
            self.reply(b"teapot", 418)
        elif url.path == "/sample":
            self.reply(b"Mite fetch sample\n")
        elif url.path == "/empty":
            self.reply(b"", 204)
        elif url.path == "/not-modified":
            self.reply(b"", 304)
        elif url.path == "/cookies":
            self.reply(b"", headers=[("set-cookie", "a=1"), ("set-cookie", "b=2")])
        elif url.path == "/compressed":
            self.server.compression_request = self.headers.get("accept-encoding")
            self.reply(gzip.compress(b"compressed"), headers=[("content-encoding", "gzip")])
        elif url.path.startswith("/redirect/") or url.path == "/loop":
            status = 302 if url.path == "/loop" else int(url.path.rsplit("/", 1)[1])
            location = "/loop" if url.path == "/loop" else parse_qs(url.query)["to"][0]
            self.reply(b"", status, [("location", location)])
        elif url.path.startswith("/release/"):
            self.server.gate(url.path.rsplit("/", 1)[1]).set()
            self.reply(b"go")
        elif url.path.startswith("/seen/"):
            key = url.path.rsplit("/", 1)[1]
            self.reply(json.dumps({"seen": "/headers/" + key in self.server.seen}).encode())
        elif url.path.startswith("/headers/"):
            key = url.path.rsplit("/", 1)[1]
            assert self.server.gate(key).wait(10), "held headers were not released"
            try:
                self.reply(b"late")
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                self.server.disconnected.add(key)
        elif url.path.startswith("/body/"):
            key = url.path.rsplit("/", 1)[1]
            size = 1048576 if key == "incremental" else 64 * 1024 * 1024
            self.send_response(200)
            self.send_header("content-length", str(size))
            self.send_header("connection", "close")
            self.end_headers()
            self.wfile.write(b"a" * 4096)
            self.wfile.flush()
            assert self.server.gate(key).wait(10), "held body was not released"
            try:
                if key == "incremental":
                    self.wfile.write(b"a" * (size - 4096))
                else:
                    # Cannot mistake draining the advertised body for cancellation.
                    for _ in range(512):
                        self.wfile.write(b"a" * 16384)
                    raise AssertionError("cancelled fetch continued receiving body")
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                self.server.disconnected.add(key)
        elif url.path == "/truncated":
            self.wfile.write(
                b"HTTP/1.1 200 OK\r\nContent-Length: 20\r\nConnection: close\r\n\r\nshort"
            )
        else:
            raise AssertionError(url.path)

    do_GET = handle_request
    do_HEAD = handle_request
    do_POST = handle_request
    do_PUT = handle_request


def network(binary, differences=False, *, native=False):
    with Peer() as first, Peer() as second:
        threads = [
            threading.Thread(target=peer.serve_forever, kwargs={"poll_interval": 0.05})
            for peer in [first, second]
        ]
        for thread in threads:
            thread.start()
        try:
            fixture = "differences.mjs" if differences else "network.mjs"
            result = subprocess.run(
                [
                    binary,
                    str(ROOT / "mite/tests/fetch" / fixture),
                    str(first.server_port),
                    str(second.server_port),
                ],
                capture_output=True,
                text=True,
                timeout=40,
            )
            assert result.returncode == 0 and not result.stderr, (
                result,
                first.errors,
                second.errors,
            )
            for key in ["suppressed"] if differences else ["aborted", "cancelled"]:
                # Joining a small control request does not synchronize the old peer.
                for _ in range(100):
                    if key in first.disconnected:
                        break
                    threading.Event().wait(0.01)
                assert key in first.disconnected, (key, first.errors)
            assert "/never" not in first.seen, first.seen
            assert not first.errors and not second.errors, (first.errors, second.errors)
            if not differences:
                example = subprocess.run(
                    [
                        binary,
                        str(ROOT / "examples/mite-fetch/client.mjs"),
                        f"http://127.0.0.1:{first.server_port}/sample",
                    ],
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                assert (
                    example.returncode == 0
                    and not example.stderr
                    and example.stdout == "status 200\nbytes 18\n"
                ), example
            if differences:
                if native:
                    assert first.compression_request == "identity", first.compression_request
            return result.stdout
        finally:
            for peer in [first, second]:
                for gate in peer.gates.values():
                    gate.set()
                peer.shutdown()
            for thread in threads:
                thread.join(5)


def tls(binary):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    certificates = ROOT / "mite/tests/tls/fixtures"
    context.load_cert_chain(
        str(certificates / "selfsigned.pem"), str(certificates / "server-key.pem")
    )
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(10)
        evidence = []

        def accept():
            try:
                peer, _ = listener.accept()
                with peer:
                    peer.settimeout(5)
                    initial = peer.recv(5, socket.MSG_PEEK)
                    assert initial[:2] == b"\x16\x03", initial
                    try:
                        with context.wrap_socket(peer, server_side=True) as secure:
                            assert not secure.recv(65536), "untrusted peer sent application data"
                    except ssl.SSLError as error:
                        assert isinstance(error, ssl.SSLEOFError) or error.reason in {
                            "TLSV1_ALERT_UNKNOWN_CA",
                            "SSLV3_ALERT_BAD_CERTIFICATE",
                            "TLSV1_ALERT_INTERNAL_ERROR",
                            "UNEXPECTED_EOF_WHILE_READING",
                        }, error
                    evidence.append("encrypted-no-http")
            except Exception as error:
                evidence.append(repr(error))

        thread = threading.Thread(target=accept, daemon=True)
        thread.start()
        result = subprocess.run(
            [binary, str(ROOT / "mite/tests/fetch/tls.mjs"), str(listener.getsockname()[1])],
            capture_output=True,
            text=True,
            timeout=15,
        )
        thread.join(10)
        assert result.returncode == 0 and not result.stderr, result
        assert (
            result.stdout == "PASS Fetch TLS certificate rejection DEPTH_ZERO_SELF_SIGNED_CERT\n"
        ), result
        assert evidence == ["encrypted-no-http"], evidence
        return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    results = [
        subprocess.run(
            [binary, str(ROOT / "mite/tests/fetch/objects.mjs")],
            capture_output=True,
            text=True,
            timeout=20,
        )
        for binary in [args.node, args.mite]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout and results[1].stdout.count("PASS ") == 12, (
        results
    )
    assert results[1].stdout.endswith("DONE Fetch objects\n"), results
    print(results[1].stdout, end="")
    outputs = [network(binary) for binary in [args.node, args.mite]]
    assert outputs[0] == outputs[1] and outputs[1].count("PASS ") == 13, outputs
    assert outputs[1].endswith("DONE Fetch network\n"), outputs
    print(outputs[1], end="")
    print("PASS runnable modern ESM Fetch example under Node and Mite")
    outputs = [
        network(args.node, differences=True),
        network(args.mite, differences=True, native=True),
    ]
    assert (
        outputs[0]
        == "compression decoded compressed\nsuppressed public event pending\nearly upload cancelled false\nDONE Fetch differences\n"
    ), outputs
    assert (
        outputs[1]
        == "compression rejected ERR_MITE_UNSUPPORTED_API\nsuppressed public event aborted\nearly upload cancelled true\nDONE Fetch differences\n"
    ), outputs
    print("PASS explicit compression and suppressed-public-event differences")
    for binary in [args.node, args.mite]:
        print(tls(binary), end="")
    result = subprocess.run(
        [args.mite, str(ROOT / "mite/tests/fetch/policies.mjs")],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert (
        result.returncode == 0
        and not result.stderr
        and result.stdout == "PASS Fetch explicit policies 7 and URL guards 3\n"
    ), result
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
