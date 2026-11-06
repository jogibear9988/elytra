#!/usr/bin/env python3
"""Compare incremental Node/Web stream bridges with real Node, without a GUI."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[2]


class Peer(ThreadingHTTPServer):
    """Own the response barriers independently of either JavaScript runtime."""

    daemon_threads = True

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.release = threading.Event()
        self.cancel_release = threading.Event()
        self.disconnected = threading.Event()
        self.errors = []
        self.upload = b""

    def handle_error(self, request, client_address):
        self.errors.append(str(sys.exc_info()[1]))


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def headers_for(self, size):
        self.connection.settimeout(10)
        self.send_response(200)
        self.send_header("Content-Length", str(size))
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def do_GET(self):
        if self.path in {"/release", "/cancel-release"}:
            event = self.server.release if self.path == "/release" else self.server.cancel_release
            event.set()
            self.headers_for(2)
            self.wfile.write(b"go")
            return
        if self.path == "/body":
            self.headers_for(1048576)
            self.wfile.write(b"a" * 4096)
            self.wfile.flush()
            assert self.server.release.wait(10), "client never observed first response block"
            self.wfile.write(b"a" * (1048576 - 4096))
            return
        if self.path == "/cancel":
            self.headers_for(64 * 1024 * 1024)
            self.wfile.write(b"a" * 4096)
            self.wfile.flush()
            assert self.server.cancel_release.wait(10), "client never cancelled first block"
            try:
                # Stop well short of Content-Length: a client which drains the
                # entire body and only then closes must not satisfy cancellation.
                for _ in range(512):
                    self.wfile.write(b"a" * 16384)
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                self.server.disconnected.set()
                return
            raise AssertionError("cancelled client kept accepting body bytes")
        raise AssertionError(self.path)

    def do_POST(self):
        assert self.path == "/upload", self.path
        self.connection.settimeout(10)
        size = int(self.headers["Content-Length"])
        self.server.upload = self.rfile.read(size)
        assert size == 262144 and self.server.upload == b"a" * size, "corrupt upload"
        body = b"262144:valid"
        self.headers_for(len(body))
        self.wfile.write(body)


def network(binary):
    with Peer() as peer:
        thread = threading.Thread(target=peer.serve_forever, kwargs={"poll_interval": 0.05})
        thread.start()
        try:
            result = subprocess.run(
                [
                    binary,
                    str(ROOT / "mite/tests/web-streams/http-adapters.mjs"),
                    str(peer.server_port),
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
            assert result.returncode == 0 and not result.stderr, (result, peer.errors)
            assert peer.disconnected.wait(5), "server did not observe cancellation"
            assert not peer.errors, peer.errors
            assert peer.upload == b"a" * 262144
            return result.stdout
        finally:
            peer.release.set()
            peer.cancel_release.set()
            peer.shutdown()
            thread.join(5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    fixture = ROOT / "mite/tests/web-streams/adapters.mjs"
    results = [
        subprocess.run([binary, str(fixture)], capture_output=True, text=True, timeout=30)
        for binary in [args.node, args.mite]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    assert results[1].stdout.count("PASS ") == 17, results
    assert results[1].stdout.endswith("DONE web adapters\n"), results
    print(results[1].stdout, end="")
    fixture = ROOT / "mite/tests/web-streams/adapter-policies.mjs"
    observed = [
        subprocess.run([binary, str(fixture)], capture_output=True, text=True, timeout=10)
        for binary in [args.node, args.mite]
    ]
    expected = [
        "invalid hwm ERR_INVALID_ARG_VALUE true\nremaining data listeners 1\nremaining drain listeners 1\nDONE adapter policies\n",
        "invalid hwm ERR_INVALID_ARG_VALUE false\nremaining data listeners 0\nremaining drain listeners 0\nDONE adapter policies\n",
    ]
    for result, output in zip(observed, expected):
        assert result.returncode == 0 and not result.stderr and result.stdout == output, observed
    print("PASS recorded option-failure ownership and terminal listener-cleanup differences")
    outputs = [network(binary) for binary in [args.node, args.mite]]
    assert (
        outputs[0]
        == outputs[1]
        == (
            "PASS independent incremental HTTP response\n"
            "PASS independent streamed HTTP upload\n"
            "PASS independent HTTP cancellation\nDONE HTTP adapters\n"
        )
    ), outputs
    print(outputs[1], end="")
    for binary in [args.node, args.mite]:
        result = subprocess.run(
            [binary, str(ROOT / "examples/mite-stream/web-bridge.mjs")],
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert result.returncode == 0 and not result.stderr, result
        assert result.stdout == "MITE WEB STREAMS\nRUN ON LIBJS\n", result
    print("PASS runnable ESM Web/Node bridge example")


if __name__ == "__main__":
    main()
