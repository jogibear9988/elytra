#!/usr/bin/env python3
"""Independent zlib fixtures and HTTP peers prove bounded native decompression without a GUI."""

import _support

import argparse
import gzip
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import zlib
from test_fetch import Handler, Peer

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = b"a" * 1048577


def raw(data):
    codec = zlib.compressobj(wbits=-15)
    return codec.compress(data) + codec.flush()


class CompressedHandler(Handler):
    def handle_request(self):
        self.connection.settimeout(5)
        self.close_connection = True
        name = self.path.strip("/")
        if name.startswith("release/"):
            self.server.gate(name.split("/")[1]).set()
            self.reply(b"go")
            return
        if name in {"partial", "cancel", "abort"}:
            codec = zlib.compressobj(wbits=31)
            first = codec.compress(PAYLOAD[:4096]) + codec.flush(zlib.Z_SYNC_FLUSH)
            rest = codec.compress(PAYLOAD[4096:]) + codec.flush()
            self.send_response(200)
            self.send_header("content-encoding", "gzip")
            self.send_header("content-length", str(len(first) + len(rest)))
            self.send_header("connection", "close")
            self.end_headers()
            self.wfile.write(first)
            self.wfile.flush()
            assert self.server.gate(name).wait(10), "client never released compressed barrier"
            if name == "partial":
                self.wfile.write(rest)
            else:
                try:
                    assert self.connection.recv(1) == b"", (
                        "unexpected request bytes after cancellation"
                    )
                except (ConnectionResetError, ConnectionAbortedError):
                    pass
                self.server.disconnected.add(name)
            return
        compressed = gzip.compress(PAYLOAD, mtime=0)
        encoding = "gzip"
        if name == "deflate":
            compressed, encoding = zlib.compress(PAYLOAD), "deflate"
        elif name == "raw":
            compressed, encoding = raw(PAYLOAD), "deflate"
        elif name == "stack":
            compressed, encoding = gzip.compress(zlib.compress(PAYLOAD), mtime=0), "deflate, gzip"
        elif name == "concat":
            compressed = (
                gzip.compress(PAYLOAD[:4096], mtime=0)
                + gzip.compress(PAYLOAD[4096:], mtime=0)
                + bytes(3)
            )
        elif name == "badcrc":
            compressed = compressed[:-8] + bytes([compressed[-8] ^ 1]) + compressed[-7:]
        elif name == "short":
            compressed = compressed[:-4]
        elif name == "empty":
            compressed = b""
        elif name == "unsupported":
            compressed, encoding = b"opaque", "br"
        else:
            assert name == "gzip", name
        self.reply(compressed, headers=[("content-encoding", encoding)])

    do_GET = handle_request


def network(binary, policy=False, unsupported=False):
    with Peer() as peer:
        peer.RequestHandlerClass = CompressedHandler
        thread = threading.Thread(target=peer.serve_forever, kwargs={"poll_interval": 0.05})
        thread.start()
        try:
            fixture = (
                "fetch-unsupported.mjs"
                if unsupported
                else "fetch-policy.mjs"
                if policy
                else "fetch.mjs"
            )
            result = subprocess.run(
                [binary, str(ROOT / "mite/tests/compression" / fixture), str(peer.server_port)],
                capture_output=True,
                text=True,
                timeout=30,
            )
            assert result.returncode == 0 and not result.stderr, (result, peer.errors)
            if not policy and not unsupported:
                for name in ["cancel", "abort"]:
                    for _ in range(100):
                        if name in peer.disconnected:
                            break
                        threading.Event().wait(0.01)
                    assert name in peer.disconnected, (name, peer.errors)
            assert not peer.errors, peer.errors
            return result.stdout
        finally:
            for gate in peer.gates.values():
                gate.set()
            peer.shutdown()
            thread.join(5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mite-inflate-") as directory:
        fixture = Path(directory) / "bytes.json"
        fixture.write_text(
            json.dumps(
                {
                    "gzip": gzip.compress(PAYLOAD, mtime=0).hex(),
                    "deflate": zlib.compress(PAYLOAD).hex(),
                    "deflate-raw": raw(PAYLOAD).hex(),
                    "text": len(PAYLOAD),
                }
            )
        )
        outputs = []
        for binary in [args.node, args.mite]:
            result = subprocess.run(
                [binary, str(ROOT / "mite/tests/compression/streams.mjs"), str(fixture)],
                capture_output=True,
                text=True,
                timeout=20,
            )
            assert result.returncode == 0 and not result.stderr, result
            outputs.append(result.stdout)
        assert (
            outputs[0] == outputs[1]
            and outputs[1].count("PASS ") == 8
            and outputs[1].endswith("DONE compression streams\n")
        ), outputs
        print(outputs[1], end="")
        expected = [
            "write accepted without reads true\npending write cancelled false\nDONE inflate pressure\n",
            "write accepted without reads false\npending write cancelled true\nDONE inflate pressure\n",
        ]
        for binary, output in zip([args.node, args.mite], expected):
            result = subprocess.run(
                [binary, str(ROOT / "mite/tests/compression/pressure.mjs"), str(fixture)],
                capture_output=True,
                text=True,
                timeout=10,
            )
            assert result.returncode == 0 and not result.stderr and result.stdout == output, result
        print("PASS explicit bounded-pressure/Node-write-acceptance difference")
        for command, stderr in [([args.node, "--expose-gc"], ""), ([args.host], "HOST_SHUTDOWN\n")]:
            result = subprocess.run(
                command + [str(ROOT / "mite/tests/compression/gc.mjs"), str(fixture)],
                capture_output=True,
                text=True,
                timeout=20,
            )
            assert (
                result.returncode == 0
                and result.stderr == stderr
                and result.stdout == "PASS forced-GC native inflater lifetime\n"
            ), result
        print("PASS forced-GC native inflater lifetime under Node/non-GUI VM host")
        sample = Path(directory) / "sample.gz"
        sample.write_bytes(gzip.compress(PAYLOAD, mtime=0))
        for binary in [args.node, args.mite]:
            result = subprocess.run(
                [binary, str(ROOT / "examples/mite-compression/decompress.mjs"), str(sample)],
                capture_output=True,
                text=True,
                timeout=10,
            )
            assert (
                result.returncode == 0 and not result.stderr and result.stdout == "bytes 1048577\n"
            ), result
        print("PASS runnable ESM decompression example under both runtimes")
    result = subprocess.run(
        [args.mite, str(ROOT / "mite/tests/compression/validation.mjs")],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert (
        result.returncode == 0
        and not result.stderr
        and result.stdout == "PASS decoder BufferSource and private-format guards 5\n"
    ), result
    print(result.stdout, end="")
    outputs = [network(binary) for binary in [args.node, args.mite]]
    assert (
        outputs[0] == outputs[1]
        and outputs[1].count("PASS ") == 4
        and outputs[1].endswith("DONE compressed Fetch\n")
    ), outputs
    print(outputs[1], end="")
    observed = [network(binary, policy=True) for binary in [args.node, args.mite]]
    assert observed == [
        "truncated gzip decoded 1048577\nDONE compressed policy\n",
        "truncated gzip rejected Z_BUF_ERROR\nDONE compressed policy\n",
    ], observed
    print("PASS explicit strict gzip trailer/Node HTTP truncation difference")
    assert (
        network(args.mite, unsupported=True)
        == "PASS unavailable content coding rejects explicitly\n"
    )
    print("PASS unavailable HTTP coding remains explicit")


if __name__ == "__main__":
    main()
