#!/usr/bin/env python3
"""Compare real HTTP upgrade ownership and byte boundaries against an owned raw peer."""

import argparse
from pathlib import Path
import shutil
import socketserver
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
HEAD = bytes([0, 255, 127, 13, 10, 128])
SWITCH = (
    b"HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: owned-protocol/1\r\n\r\n"
)


class Peer(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        try:
            line = self.rfile.readline(8192).split()
            if not line:
                return
            path = line[1]
            while self.rfile.readline(32768) not in (b"\r\n", b""):
                pass
            if path == b"/normal":
                self.request.sendall(
                    b"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\nok!"
                )
            elif path == b"/reject":
                self.request.sendall(
                    b"HTTP/1.1 403 Denied\r\nConnection: close\r\nContent-Length: 6\r\n\r\ndenied"
                )
            elif path == b"/no-listener":
                self.request.sendall(SWITCH)
                if self.request.recv(1):
                    raise AssertionError("Unobserved upgraded socket was not closed")
            else:
                if path == b"/split":
                    self.request.sendall(SWITCH[:24])
                    time.sleep(0.02)
                    self.request.sendall(SWITCH[24:-2])
                    time.sleep(0.02)
                    self.request.sendall(SWITCH[-2:] + HEAD)
                else:
                    self.request.sendall(SWITCH + HEAD)
                if self.rfile.read(4) != b"ping":
                    raise AssertionError("Upgraded peer did not receive protocol bytes")
                self.request.sendall(b"reply")
                while self.request.recv(4096):
                    pass
        except (ConnectionResetError, BrokenPipeError):
            pass
        except Exception as error:
            self.server.errors.append(type(error).__name__)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def run(binary):
    with Server(("127.0.0.1", 0), Peer) as server:
        server.errors = []
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            result = subprocess.run(
                [
                    str(binary),
                    str(ROOT / "mite/tests/http-upgrade/main.mjs"),
                    str(server.server_address[1]),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=30,
            )
        finally:
            server.shutdown()
            thread.join()
        markers = [line for line in result.stdout.splitlines() if line.startswith("PASS ")]
        if result.returncode or len(markers) != 7 or server.errors:
            raise RuntimeError(
                f"{binary}: exit={result.returncode}, peer errors={server.errors}\n{result.stdout}\n{result.stderr}"
            )
        print(f"{Path(binary).name}: HTTP upgrade: 7 groups PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.reference_only and not args.mite:
        parser.error("--mite required unless --reference-only")
    node = shutil.which("node")
    if not node:
        parser.error("Node reference required")
    run(node)
    if not args.reference_only:
        run(args.mite.resolve())


if __name__ == "__main__":
    main()
