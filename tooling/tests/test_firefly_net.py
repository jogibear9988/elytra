#!/usr/bin/env python3
"""Run public net HTTP(S) acceptance against owned independent peers; no windows/settings changes."""

import _support

import argparse
import gzip
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
import shutil
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[2]


class Peer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.lock = threading.Lock()
        self.seen = set()
        self.gates = {}
        self.errors = []
        self.untrusted_http = False

    def gate(self, key):
        with self.lock:
            return self.gates.setdefault(key, threading.Event())

    def handle_error(self, *_args):
        self.errors.append("Independent HTTP handler failed")


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def body(self):
        if self.headers.get("transfer-encoding") == "chunked":
            chunks = []
            while True:
                size = int(self.rfile.readline().split(b";", 1)[0], 16)
                if not size:
                    while self.rfile.readline() != b"\r\n":
                        pass
                    return b"".join(chunks)
                chunks.append(self.rfile.read(size))
                assert self.rfile.read(2) == b"\r\n"
                assert sum(map(len, chunks)) <= 4 * 1024 * 1024
        return self.rfile.read(int(self.headers.get("content-length", "0")))

    def reply(self, body, code=200, headers=()):
        self.send_response(code)
        self.send_header("content-length", str(len(body)))
        self.send_header("connection", "close")
        self.send_header("x-peer", "owned-loopback")
        for name, value in headers:
            self.send_header(name, value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def handle_request(self):
        self.connection.settimeout(12)
        self.close_connection = True
        url = urlsplit(self.path)
        with self.server.lock:
            self.server.seen.add(url.path)
        body = self.body()
        if url.path == "/echo":
            self.reply(
                json.dumps(
                    {
                        "method": self.command,
                        "bodyHex": body.hex(),
                        "authorization": self.headers.get("authorization"),
                        "probe": self.headers.get("x-probe"),
                    }
                ).encode(),
                headers=[("content-type", "application/json")],
            )
        elif url.path == "/gzip":
            self.reply(
                gzip.compress("decoded Grüße 日本語".encode()),
                headers=[("content-encoding", "gzip")],
            )
        elif url.path == "/redirect":
            self.reply(b"", 302, [("location", parse_qs(url.query)["to"][0])])
        elif url.path.startswith("/hold/"):
            key = url.path.split("/")[-1]
            assert self.server.gate(key).wait(12), "Owned request barrier not released"
            try:
                self.reply(b"released")
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass
        elif url.path.startswith("/stream/"):
            key = url.path.split("/")[-1]
            self.send_response(200)
            self.send_header("content-length", str(1024 * 1024))
            self.send_header("connection", "close")
            self.end_headers()
            try:
                self.wfile.write(b"x" * 4096)
                self.wfile.flush()
                assert self.server.gate(key).wait(12), "Owned streaming barrier not released"
                self.wfile.write(b"x" * (1024 * 1024 - 4096))
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass
        elif url.path.startswith("/seen/"):
            with self.server.lock:
                seen = "/hold/" + url.path.split("/")[-1] in self.server.seen
            self.reply(json.dumps({"seen": seen}).encode())
        elif url.path.startswith("/release/"):
            self.server.gate(url.path.split("/")[-1]).set()
            self.reply(b"released")
        elif url.path == "/large":
            self.reply(bytes(range(256)) * 8192)
        elif url.path == "/untrusted":
            self.server.untrusted_http = True
            self.reply(b"TLS verification was bypassed")
        else:
            self.reply(b"not found", 404)

    do_GET = handle_request
    do_POST = handle_request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--offscreen", action="store_true")
    parser.add_argument("--electron", type=Path)
    parser.add_argument("--native-probe", type=Path)
    parser.add_argument("--code-oss", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.firefly and not args.electron and not args.native_probe:
        parser.error("Provide --firefly, --electron and/or --native-probe")
    if args.code_oss and (not args.native_probe or not args.node):
        parser.error("--code-oss requires --native-probe and Node")
    peers = [Peer(), Peer(), Peer()]
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    certs = ROOT / "mite/tests/tls/fixtures"
    context.load_cert_chain(str(certs / "selfsigned.pem"), str(certs / "server-key.pem"))
    peers[2].socket = context.wrap_socket(peers[2].socket, server_side=True)
    threads = [threading.Thread(target=peer.serve_forever, daemon=True) for peer in peers]
    for thread in threads:
        thread.start()
    config = {
        "origin": f"http://127.0.0.1:{peers[0].server_port}",
        "other": f"http://localhost:{peers[1].server_port}",
        "crossPort": f"http://127.0.0.1:{peers[1].server_port}",
        "secure": f"https://127.0.0.1:{peers[2].server_port}",
    }
    try:
        with tempfile.TemporaryDirectory(prefix="elytra-public-net-") as directory:
            work = Path(directory)
            for name in ["main.mjs", "utility.mjs", "transport.mjs"]:
                (work / name).write_bytes((ROOT / "firefly/tests/net" / name).read_bytes())
            for label, binary in [("Electron", args.electron), ("Firefly", args.firefly)]:
                if binary is None:
                    continue
                for name in ["main.mjs", "utility.mjs"]:
                    source = (ROOT / "firefly/tests/net" / name).read_text()
                    (work / name).write_text(
                        source.replace("from 'firefly'", "from 'electron'")
                        if label == "Electron"
                        else source
                    )
                env = {
                    **os.environ,
                    "ELYTRA_NET_TEST": json.dumps(
                        {**config, "strictOrigin": label == "Firefly", "runId": label.lower()}
                    ),
                }
                if args.offscreen and label == "Firefly":
                    env["QT_QPA_PLATFORM"] = "offscreen"
                result = subprocess.run(
                    [str(binary.resolve()), str(work / "main.mjs")],
                    env=env,
                    cwd=work,
                    capture_output=True,
                    text=True,
                    timeout=25,
                )
                assert (
                    result.returncode == 0
                    and "PASS public main net transport" in result.stdout
                    and "PASS public utility net transport" in result.stdout
                ), (label, result)
                assert "VERIFICATION FAILED" not in result.stderr, (label, result)
                print(label + ": " + result.stdout.strip(), flush=True)
            if args.native_probe:
                env = {
                    **os.environ,
                    "ELYTRA_NET_TEST": json.dumps(
                        {**config, "strictOrigin": True, "runId": "native"}
                    ),
                }
                result = subprocess.run(
                    [str(args.native_probe.resolve()), str(ROOT / "firefly/tests/net/native.mjs")],
                    env=env,
                    cwd=work,
                    capture_output=True,
                    text=True,
                    timeout=20,
                )
                assert (
                    result.returncode == 0
                    and not result.stderr
                    and "PASS actual net factory/exact Host/captured Mite HTTP(S) transport"
                    in result.stdout
                ), result
                print(result.stdout.strip(), flush=True)
                if args.code_oss:
                    for probe in ["probe-net", "probe-net-request"]:
                        result = subprocess.run(
                            [
                                args.node,
                                str(ROOT / "tooling/code_oss" / (probe + ".mjs")),
                                str(args.code_oss.resolve()),
                                str(args.native_probe.resolve()),
                            ],
                            env=env,
                            cwd=work,
                            capture_output=True,
                            text=True,
                            timeout=25,
                        )
                        assert result.returncode == 0 and not result.stderr, (probe, result)
                        print(result.stdout.strip(), flush=True)
            assert not peers[2].untrusted_http, "HTTPS verification failure sent an HTTP request"
            assert all(not peer.errors for peer in peers), [peer.errors for peer in peers]
    finally:
        for peer in peers:
            for gate in peer.gates.values():
                gate.set()
            peer.shutdown()
            peer.server_close()
        for thread in threads:
            thread.join(timeout=2)


if __name__ == "__main__":
    main()
