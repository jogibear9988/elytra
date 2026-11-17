#!/usr/bin/env python3
"""Validate Node/Mite WebSocket bytes against an independent local TCP/TLS protocol peer."""

import argparse
import base64
import hashlib
import os
from pathlib import Path
import shutil
import socketserver
import ssl
import struct
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[2]
GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def frame(opcode, payload=b"", fin=True, masked=False, rsv=False):
    first = opcode | (128 if fin else 0) | (64 if rsv else 0)
    n = len(payload)
    head = bytes([first, (128 if masked else 0) | (n if n < 126 else 126 if n < 65536 else 127)])
    if n >= 65536:
        head += struct.pack("!Q", n)
    elif n >= 126:
        head += struct.pack("!H", n)
    if masked:
        key = b"abcd"
        return head + key + bytes(value ^ key[index % 4] for index, value in enumerate(payload))
    return head + payload


class Peer(socketserver.BaseRequestHandler):
    def handle(self):
        self.server.accepted.set()
        stream = self.request
        stream.settimeout(10)
        try:
            if self.server.tls:
                try:
                    stream = self.server.tls.wrap_socket(stream, server_side=True)
                except ssl.SSLError:
                    if self.server.mode == "tls-reject":
                        return
                    raise
            if self.server.mode == "tls-reject":
                try:
                    assert not stream.recv(1), "Untrusted TLS peer received HTTP bytes"
                except ssl.SSLError:
                    pass  # The client aborts after certificate verification, before HTTP.
                return
            self.stream = stream
            self.input = stream.makefile("rb")
            self.pongs = []
            line = self.input.readline(8192).split()
            if not line and self.server.mode == "connecting-close":
                return
            assert len(line) == 3 and line[0] == b"GET" and line[2] == b"HTTP/1.1", "request line"
            headers = {}
            total = len(line)
            while True:
                line = self.input.readline(32769)
                total += len(line)
                assert line and total < 32768, "bounded HTTP head"
                if line == b"\r\n":
                    break
                name, value = line.split(b":", 1)
                headers[name.lower()] = value.strip()
            assert headers[b"upgrade"].lower() == b"websocket", "upgrade"
            assert b"upgrade" in headers[b"connection"].lower(), "connection"
            assert headers[b"sec-websocket-version"] == b"13", "version"
            assert len(base64.b64decode(headers[b"sec-websocket-key"], validate=True)) == 16, (
                "nonce"
            )
            accept = base64.b64encode(hashlib.sha1(headers[b"sec-websocket-key"] + GUID).digest())
            mode = self.server.mode
            if mode == "bad-accept":
                accept = b"bad"
            head = (
                b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
                + accept
                + b"\r\n"
            )
            if mode == "happy":
                assert headers[b"sec-websocket-protocol"] == b"owned", "subprotocol"
                head += b"Sec-WebSocket-Protocol: owned\r\n"
            head += b"\r\n"
            if mode == "happy":
                stream.sendall(head + frame(1, b"early"))
                # Real fragmented message with an interleaved ping control frame.
                stream.sendall(
                    frame(1, b"a", False)
                    + frame(9, b"p")
                    + frame(0, b"b")
                    + frame(2, bytes([0, 255, 7]))
                )
                for expected in [
                    (1, b"hello"),
                    (2, bytes([0, 255, 17])),
                    (2, b"blob"),
                    (1, b"after"),
                ]:
                    assert self.message() == expected, "client FIFO/snapshot"
                assert self.message() == (1, b"array"), "array request"
                stream.sendall(frame(2, bytes([9, 8])))
                self.finish_close("done")
                assert b"p" in self.pongs, "automatic pong"
            elif mode == "blob-close":
                stream.sendall(head)
                if self.server.mite:
                    assert self.message() == (2, b"pending"), "Mite preserves admitted pending Blob"
                    assert self.message() == (1, b"tail"), "Mite pending Blob FIFO"
                    self.finish_close("blob-done")
                else:
                    # Node 24.15 closes immediately, dropping these queued Blob sends.
                    self.finish_close("blob-done")
            elif mode == "send-bound":
                assert self.server.mite, "Private queue bound is Mite-only"
                stream.sendall(head)
                received = 0
                while True:
                    opcode, payload = self.message()
                    if opcode == 8:
                        assert 1 <= received <= 8, "private admitted frame bound"
                        assert payload == struct.pack("!H", 1000) + f"bound-{received}".encode(), (
                            "all admitted frames precede close exactly once"
                        )
                        stream.sendall(frame(8, payload))
                        break
                    assert opcode == 2 and len(payload) == 512 * 1024, "bounded binary frame"
                    assert received < 8, "no false admission beyond private budget"
                    expected = bytes((index * 7 + received) & 255 for index in range(len(payload)))
                    assert payload == expected, "admitted frame FIFO and byte snapshot"
                    received += 1
            elif mode == "tls-trusted":
                stream.sendall(head)
                assert self.message() == (1, b"secure"), "secure data"
                stream.sendall(frame(1, b"secure-ok"))
                self.finish_close("done")
            elif mode == "connecting-close":
                stream.sendall(head)
                while stream.recv(4096):
                    pass
            elif mode == "abrupt":
                stream.sendall(head)
            elif mode == "bad-accept":
                stream.sendall(head)
                while stream.recv(4096):
                    pass
            else:
                invalid = {
                    "masked": frame(1, b"bad", masked=True),
                    "rsv": frame(1, b"bad", rsv=True),
                    "utf8": frame(1, b"\xff"),
                    "oversize": bytes([0x82, 127]) + struct.pack("!Q", 1024 * 1024 + 1),
                }[mode]
                stream.sendall(head + invalid)
                try:
                    opcode, payload = self.message()
                    if opcode == 8:
                        stream.sendall(frame(8, payload))
                except (EOFError, ConnectionResetError, BrokenPipeError):
                    pass
        except (EOFError, ConnectionResetError, BrokenPipeError):
            if self.server.mode not in [
                "bad-accept",
                "masked",
                "rsv",
                "utf8",
                "oversize",
                "tls-reject",
                "connecting-close",
            ]:
                self.server.errors.append("unexpected peer close")
        except Exception as error:
            self.server.errors.append(type(error).__name__ + ": " + str(error))
        finally:
            try:
                stream.close()
            except OSError:
                pass
            self.server.done.set()

    def exact(self, size):
        data = self.input.read(size)
        if len(data) != size:
            raise EOFError()
        return data

    def message(self):
        parts = []
        kind = None
        while True:
            a, b = self.exact(2)
            assert not a & 112, "client RSV"
            assert b & 128, "client masking required"
            size = b & 127
            if size == 126:
                size = struct.unpack("!H", self.exact(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self.exact(8))[0]
            assert size <= 1024 * 1024, "bounded client frame"
            key = self.exact(4)
            payload = bytes(value ^ key[index % 4] for index, value in enumerate(self.exact(size)))
            opcode = a & 15
            if opcode == 10:
                self.pongs.append(payload)
                continue
            if opcode == 9:
                self.stream.sendall(frame(10, payload))
                continue
            if opcode == 8:
                return opcode, payload
            if opcode:
                assert kind is None
                kind = opcode
            else:
                assert kind is not None
            parts.append(payload)
            if a & 128:
                return kind, b"".join(parts)

    def finish_close(self, reason):
        opcode, payload = self.message()
        assert opcode == 8, "close frame"
        assert payload == struct.pack("!H", 1000) + reason.encode(), "close code/reason"
        self.stream.sendall(frame(8, payload))


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def run(binary, mode, trusted=False, mite=False):
    tls = None
    if mode.startswith("tls-"):
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(
            ROOT / "mite/tests/tls/fixtures/server.pem",
            ROOT / "mite/tests/tls/fixtures/server-key.pem",
        )
    with Server(("127.0.0.1", 0), Peer) as server:
        server.mode = mode
        server.mite = mite
        server.tls = tls
        server.errors = []
        server.done = threading.Event()
        server.accepted = threading.Event()
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        env = os.environ.copy()
        env.pop("NODE_TLS_REJECT_UNAUTHORIZED", None)
        env.pop("NODE_EXTRA_CA_CERTS", None)
        if trusted:
            env["NODE_EXTRA_CA_CERTS"] = str(ROOT / "mite/tests/tls/fixtures/ca.pem")
        url = f"{'wss' if tls else 'ws'}://127.0.0.1:{server.server_address[1]}/{mode}"
        try:
            result = subprocess.run(
                [str(binary), str(ROOT / "mite/tests/websocket/main.mjs"), mode, url],
                capture_output=True,
                text=True,
                env=env,
                timeout=20,
                cwd=ROOT,
            )
            assert (
                result.returncode == 0
                and result.stdout.strip() == "PASS standalone WebSocket " + mode
            ), (binary, mode, result.returncode, result.stdout, result.stderr, server.errors)
            if mode != "connecting-close" or server.accepted.is_set():
                assert server.done.wait(2), "peer did not finish"
            assert not server.errors, (binary, mode, server.errors)
            print(f"{Path(binary).name}: WebSocket {mode} PASS", flush=True)
        finally:
            server.shutdown()
            worker.join(timeout=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node:
        parser.error("Node reference required")
    if not args.reference_only and not args.mite:
        parser.error("--mite required")
    for binary in [args.node] + ([] if args.reference_only else [args.mite.resolve()]):
        events = subprocess.run(
            [str(binary), str(ROOT / "mite/tests/websocket/events.mjs")],
            capture_output=True,
            text=True,
            timeout=10,
            cwd=ROOT,
        )
        assert events.returncode == 0 and events.stdout.count("PASS ") == 3, (
            binary,
            events.returncode,
            events.stdout,
            events.stderr,
        )
        print(f"{Path(binary).name}: WebSocket event/constructor policy 3 groups PASS", flush=True)
        for mode in [
            "happy",
            "connecting-close",
            "blob-close",
            "abrupt",
            "bad-accept",
            "masked",
            "rsv",
            "utf8",
            "tls-reject",
            "tls-trusted",
        ]:
            run(
                binary,
                mode,
                mode == "tls-trusted",
                not args.reference_only and binary == args.mite.resolve(),
            )
        if not args.reference_only and binary == args.mite.resolve():
            run(binary, "oversize", mite=True)
            run(binary, "send-bound", mite=True)


if __name__ == "__main__":
    main()
