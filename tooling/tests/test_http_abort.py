#!/usr/bin/env python3
"""Compare HTTP signal lifecycles and independently verify native TCP closure."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import socketserver
import subprocess
import threading


class Peer(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(8)
        try:
            line = self.rfile.readline(8192)
            assert line.startswith(
                b"POST /upload " if self.server.mode == "upload" else b"GET /response "
            ), line
            size = len(line)
            while True:
                line = self.rfile.readline(8192)
                size += len(line)
                assert line and size < 65536
                if line == b"\r\n":
                    break
            self.server.received.set()
            self.request.sendall(
                b"HTTP/1.1 100 Continue\r\n\r\n"
                if self.server.mode == "upload"
                else b"HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\npartial"
            )
            while self.request.recv(65536):
                pass
            self.server.closed.set()
        except ConnectionResetError:
            self.server.closed.set()
        except Exception as error:
            self.server.failure = repr(error)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    root = Path(__file__).resolve().parents[2] / "mite/tests/http-abort"
    expected = {
        "paused": "paused true true true true\n",
        "deadline": "deadline true\n",
        "server-listen": "server listen graceful true true\n",
        "socket-event": "socket event socket,error,close true\n",
        "url": "url true true\n",
        "finish": "after finish true true true\n",
        "immediate": "immediate 0 0 1 true true\n",
        "lookup": "lookup 0 1 true true\n",
        "pre": "pre returned:true,error true true false\n",
        "propagation": "propagation true\n",
        "response": "response abort response,data,request:error,aborted,request:close,response:ECONNRESET,response:close true false false\n",
        "settled": "settled done true 0 false\n",
        "validation": "validation 10\n",
    }
    for file in sorted(root.glob("*.mjs")):
        if file.stem == "raw-peer":
            continue
        results = [
            subprocess.run([exe, str(file)], capture_output=True, text=True, timeout=20)
            for exe in [args.node, args.mite]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), (
            file.name,
            results,
        )
        if file.stem == "synthetic":
            assert results[0].stdout == "synthetic HTTP true true\n", results
            assert results[1].stdout == "synthetic HTTP false true\n", results
        else:
            assert results[0].stdout == results[1].stdout == expected[file.stem], (
                file.name,
                results,
            )
        print("PASS HTTP cancellation:", file.name, flush=True)
    for mode in ["response", "upload"]:
        results = []
        for exe in [args.node, args.mite]:
            with Server(("127.0.0.1", 0), Peer) as server:
                server.mode = mode
                server.received = threading.Event()
                server.closed = threading.Event()
                server.failure = None
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                try:
                    result = subprocess.run(
                        [exe, str(root / "raw-peer.mjs"), str(server.server_address[1]), mode],
                        capture_output=True,
                        text=True,
                        timeout=20,
                    )
                    assert result.returncode == 0 and not result.stderr, result
                    assert server.received.wait(2) and server.closed.wait(2), (
                        mode,
                        result,
                        server.failure,
                    )
                    assert server.failure is None, server.failure
                    results.append(result.stdout)
                finally:
                    server.shutdown()
                    thread.join(timeout=3)
        assert results == [f"peer {mode} 1 true true\n"] * 2, results
        print("PASS independent OS peer closed after cancellation:", mode, flush=True)


if __name__ == "__main__":
    main()
