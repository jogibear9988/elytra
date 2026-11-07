#!/usr/bin/env python3
"""Check actual regular files, bounded native child streaming and explicit failures."""

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import struct
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    probe = args.probe.resolve()
    with tempfile.TemporaryDirectory(prefix="firefly files ä ") as temporary:
        directory = Path(temporary)
        path = directory / "body 世界.bin"
        length = 4 * 1024 * 1024 + 19
        pattern = bytes((i * 37 + 11) & 255 for i in range(256))
        path.write_bytes((pattern * (length // 256 + 1))[:length])

        def run(operation, value):
            result = subprocess.run(
                [str(probe), operation, str(value)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=30,
            )
            assert result.returncode == 0, result
            if operation not in ("metadata", "metadata-head"):
                print(result.stdout.strip())
            return result.stdout

        def packet(kind, epoch=101, request=7, offset=0, payload=b""):
            return (
                struct.pack("<4sBBHQIIQ", b"FFRP", 1, kind, 0, epoch, request, len(payload), offset)
                + payload
            )

        def check_packet(source, accepted):
            result = subprocess.run(
                [str(probe), "packet-decode", "-"], input=source, capture_output=True, timeout=10
            )
            assert (result.returncode == 0) == accepted, result
            if accepted:
                assert result.stdout == source, result

        encoded = subprocess.run(
            [str(probe), "packet-encode", "-"], capture_output=True, timeout=10
        )
        assert encoded.returncode == 0, encoded
        assert encoded.stdout == packet(
            2, 0x0102030405060708, 0x10203040, 123, bytes([0, 128, 255])
        )
        for source in [
            packet(1, payload=b"{}"),
            packet(1, payload=b"x" * 32768),
            packet(2, payload=b"x" * 65536),
            packet(2, offset=64 * 1024**3 - 1, payload=b"x"),
            packet(3, offset=64 * 1024**3),
            packet(4),
            packet(2, epoch=2**64 - 1, request=2**32 - 1, payload=b"x"),
        ]:
            check_packet(source, True)
        for source in [
            packet(0),
            packet(5),
            packet(1),
            packet(2),
            packet(3, payload=b"x"),
            packet(4, offset=1),
            packet(4, payload=b"x"),
            packet(2, epoch=0, payload=b"x"),
            packet(2, request=0, payload=b"x"),
            packet(2, offset=64 * 1024**3, payload=b"x"),
            packet(3, offset=2**64 - 1),
            packet(1, offset=1, payload=b"x"),
            packet(1, payload=b"x" * 32769),
            packet(2, payload=b"x" * 65537),
        ]:
            check_packet(source, False)
        valid = packet(2, payload=b"abc")
        for position in [0, 4, 5, 6, 7, 20]:
            corrupt = bytearray(valid)
            corrupt[position] = 255
            check_packet(corrupt, False)
        for truncated_length in [0, 4, 20, 31, len(valid) - 1]:
            check_packet(valid[:truncated_length], False)
        check_packet(valid + b"trailing", False)
        print(
            "PASS independent file packet endian/layout, owner/request/offset identity, lengths, kinds and budgets"
        )

        metadata = json.loads(run("metadata", path))
        assert metadata == {
            "schema": 1,
            "kind": "file-response",
            "request": 7,
            "status": 200,
            "length": length,
            "head": False,
            "headers": [
                ["Cross-Origin-Opener-Policy", "same-origin"],
                ["Cross-Origin-Embedder-Policy", "require-corp"],
                ["Cache-Control", "no-cache, no-store"],
                ["Document-Policy", "js-profiling"],
                ["Set-Cookie", "first=1"],
                ["Set-Cookie", "second=2"],
                ["X-Bytes", "\x80\xff"],
                ["Content-Type", "application/octet-stream"],
                ["Content-Length", str(length)],
            ],
        }, metadata
        assert str(directory) not in json.dumps(metadata, ensure_ascii=False)
        head_metadata = json.loads(run("metadata-head", path))
        assert head_metadata == metadata | {"head": True}

        def decode(value, accepted):
            source = value if isinstance(value, str) else json.dumps(value)
            result = subprocess.run(
                [str(probe), "decode", "-"],
                input=source,
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=10,
            )
            assert (result.returncode == 0) == accepted, result
            if accepted:
                assert json.loads(result.stdout) == value, result

        decode(metadata, True)
        decode(head_metadata, True)
        decode(metadata | {"request": 2**32 - 1}, True)
        maximum = copy.deepcopy(metadata)
        maximum["length"] = 64 * 1024**3
        maximum["headers"][-1][1] = str(maximum["length"])
        decode(maximum, True)
        for field, value in [
            ("schema", 2),
            ("kind", "request"),
            ("request", 0),
            ("request", -1),
            ("request", 2**32),
            ("request", "7"),
            ("status", 204),
            ("length", -1),
            ("length", 64 * 1024**3 + 1),
            ("length", 1.5),
            ("head", "false"),
            ("path", str(path)),
            ("headers", {}),
        ]:
            decode(metadata | {field: value}, False)
        for pair in [
            ["Invalid Name", "x"],
            ["x-name", "a\r\nb"],
            ["x-name", "a\0b"],
            ["x-name", " leading"],
            ["x-name", "\u0100"],
            ["Transfer-Encoding", "chunked"],
            ["Connection", "close"],
            ["TE", "trailers"],
            ["Content-Length", str(length)],
            ["content-type", "text/plain"],
            ["x-name"],
            [1, "value"],
        ]:
            value = copy.deepcopy(metadata)
            value["headers"].append(pair)
            decode(value, False)
        for name in ["Content-Length", "Content-Type"]:
            value = copy.deepcopy(metadata)
            value["headers"] = [pair for pair in value["headers"] if pair[0] != name]
            decode(value, False)
        value = copy.deepcopy(metadata)
        value["headers"][-1][1] = str(length + 1)
        decode(value, False)
        value = copy.deepcopy(metadata)
        value["headers"] += [["X", "v"]] * (64 - len(value["headers"]))
        decode(value, True)
        value["headers"].append(["X", "v"])
        decode(value, False)
        decode(json.dumps(metadata) + "\0", False)
        decode(json.dumps(metadata) + "\x1a", False)
        decode("x" * (32 * 1024 + 1), False)
        decode("[" * 600 + "]" * 600, False)
        for suffix, mime in [
            ("mjs", "application/javascript"),
            ("HTML", "text/html"),
            ("css", "text/css"),
            ("wasm", "application/wasm"),
            ("json", "application/json"),
            ("unknown", "application/octet-stream"),
        ]:
            resource = directory / f"mime.{suffix}"
            resource.touch()
            fields = dict(json.loads(run("metadata", resource))["headers"])
            assert fields["Content-Type"] == mime and fields["Content-Length"] == "0", fields
        print(
            "PASS independent JSON metadata, exact schema/header/length/budget refusals, HEAD and upstream MIME inference"
        )

        run("metadata-policy", path)
        assert "bytes=0 chunks=0" in run("transfer-head", path)
        assert "bytes=65536 chunks=1" in run("transfer-cancel", path)
        assert "peer loss to separate native child: bytes=65536 chunks=1" in run(
            "transfer-peer-loss", path
        )
        streamed = run("transfer", path)
        assert f"bytes={length} chunks=65" in streamed, streamed
        assert f"HEAD length={length}" in run("head", path)
        run("cancel", path)
        run("refuse", directory)
        run("refuse", directory / "missing")
        run("refuse", "relative.bin")
        empty = directory / "empty"
        empty.touch()
        assert "bytes=0 chunks=0" in run("transfer", empty)
        if os.name != "nt":
            fifo = directory / "fifo"
            os.mkfifo(fifo)
            run("refuse", fifo)
            run("refuse", "/dev/null")
            huge = directory / "oversized"
            with huge.open("wb") as file:
                file.truncate(64 * 1024 * 1024 * 1024 + 1)
            run("refuse", huge)
        requests = directory / "requests.bin"
        requests.write_bytes(pattern * 32)
        run("requests", requests)
        run("grow", path)
        run("replace", path)
        run("truncate", path)
    print(
        "PASS native file-body component; renderer requests and public protocol remain separate integration work"
    )


if __name__ == "__main__":
    main()
