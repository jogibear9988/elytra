#!/usr/bin/env python3
"""Compare Blob/File/FormData and inspect multipart payloads with Python's MIME parser."""

import _support  # Standalone sibling/build helper imports.

import argparse
from email import policy
from email.parser import BytesParser
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import threading
from test_fetch import Handler, Peer

ROOT = Path(__file__).resolve().parents[2]


class DataHandler(Handler):
    def handle_request(self):
        self.connection.settimeout(10)
        self.close_connection = True
        body = self.read_body()
        declared = self.headers.get("content-length")
        content_type = self.headers.get("content-type")
        self.server.payloads.append((self.path, body, content_type))
        if self.path in {"/blob", "/redirect-blob"}:
            assert declared is not None and int(declared) == len(body), "Blob length mismatch"
            if self.path == "/redirect-blob":
                self.reply(b"", 307, [("location", "/blob")])
            else:
                self.reply(
                    json.dumps(
                        {
                            "length": len(body),
                            "declared": int(declared),
                            "type": content_type,
                            "hex": body.hex(),
                        }
                    ).encode()
                )
        elif self.path in {"/form", "/redirect-form", "/sample"}:
            assert declared is not None and int(declared) == len(body), "Multipart length mismatch"
            message = BytesParser(policy=policy.default).parsebytes(
                b"Content-Type: " + content_type.encode() + b"\r\nMIME-Version: 1.0\r\n\r\n" + body
            )
            if not message.is_multipart():
                assert self.server.replay_policy and self.path == "/form", content_type
                self.server.invalid_multipart += 1
                self.reply(json.dumps({"validBoundary": False}).encode())
                return
            parts = []
            for part in message.iter_parts():
                value = part.get_payload(decode=True)
                parts.append(
                    {
                        "name": part.get_param("name", header="content-disposition"),
                        "filename": part.get_filename(),
                        "size": len(value),
                        "sha256": hashlib.sha256(value).hexdigest(),
                        "text": value.decode("utf-8") if part.get_filename() is None else None,
                    }
                )
            self.server.forms.append(parts)
            if self.path == "/redirect-form":
                self.reply(b"", 307, [("location", "/form")])
            else:
                self.reply(
                    json.dumps(
                        {"validLength": True, "validBoundary": True, "parts": parts}
                    ).encode()
                )
        elif self.path == "/response-form":
            body = (
                b'--response-boundary\r\nContent-Disposition: form-data; name="field"\r\n\r\n'
                + b"a" * 1100000
                + '\r\n--response-boundary\r\nContent-Disposition: form-data; name="file"; filename="dir/é.txt"\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: base64\r\n\r\naGVsbG8=\r\n--response-boundary\r\nContent-Disposition: form-data; name="field"\r\n\r\nlast\r\n--response-boundary--\r\n'.encode()
            )
            self.reply(
                body,
                headers=[("content-type", 'Multipart/Form-Data; boundary="response-boundary"')],
            )
        elif self.path == "/response-blob":
            self.reply(bytes([0, 1, 255]), headers=[("content-type", "application/octet-stream")])
        elif self.path == "/response-urlencoded":
            self.reply(
                b"field=one&field=two&space=a+b",
                headers=[("content-type", "application/x-www-form-urlencoded")],
            )
        else:
            raise AssertionError(self.path)

    do_GET = handle_request
    do_POST = handle_request


def network(binary, replay_policy=False):
    with Peer() as peer:
        peer.RequestHandlerClass = DataHandler
        peer.payloads = []
        peer.forms = []
        peer.replay_policy = replay_policy
        peer.invalid_multipart = 0
        thread = threading.Thread(target=peer.serve_forever, kwargs={"poll_interval": 0.05})
        thread.start()
        try:
            result = subprocess.run(
                [
                    binary,
                    str(
                        ROOT
                        / "mite/tests/fetch"
                        / ("data-replay.mjs" if replay_policy else "data-network.mjs")
                    ),
                    str(peer.server_port),
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
            assert result.returncode == 0 and not result.stderr, (result, peer.errors)
            assert not peer.errors, peer.errors
            if replay_policy:
                assert (
                    len(peer.payloads) == 2
                    and peer.payloads[0][0] == "/redirect-form"
                    and peer.payloads[1][0] == "/form"
                ), peer.payloads
                if result.stdout.startswith("multipart replay boundary true"):
                    assert not peer.invalid_multipart and peer.forms[0] == peer.forms[1]
                    assert peer.payloads[0][1:] == peer.payloads[1][1:], (
                        "Replay changed body/boundary"
                    )
                else:
                    assert (
                        peer.invalid_multipart == 1 and peer.payloads[0][1] != peer.payloads[1][1]
                    ), "Missing reference boundary defect"
            else:
                original = next(body for path, body, _ in peer.payloads if path == "/redirect-blob")
                replay = [body for path, body, _ in peer.payloads if path == "/blob"][-1]
                assert original == replay == b"replay"
                example = subprocess.run(
                    [
                        binary,
                        str(ROOT / "examples/mite-fetch/multipart.mjs"),
                        f"http://127.0.0.1:{peer.server_port}/sample",
                    ],
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                assert example.returncode == 0 and not example.stderr, example
                status, body = example.stdout.strip().split("\n")
                assert status == "status 200"
                parts = json.loads(body)["parts"]
                assert parts[0]["name"] == "project" and parts[0]["text"] == "Mite + Firefly"
                assert parts[1]["filename"] == "project.json" and parts[1]["size"] == len(
                    b'{"runtime":"LibJS"}'
                )
                assert not peer.errors, peer.errors
            return result.stdout
        finally:
            peer.shutdown()
            thread.join(5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    for fixture, count, marker in [
        ("data.mjs", 13, "DONE Fetch data\n"),
        ("codec-slices.mjs", 0, "DONE codec slices 48\n"),
    ]:
        results = [
            subprocess.run(
                [binary, str(ROOT / "mite/tests/fetch" / fixture)],
                capture_output=True,
                text=True,
                timeout=20,
            )
            for binary in [args.node, args.mite]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), results
        assert (
            results[0].stdout == results[1].stdout and results[1].stdout.count("PASS ") == count
        ), results
        assert results[1].stdout.endswith(marker), results
        print(
            results[1].stdout if count else "PASS 48 parser Buffer codec comparisons",
            end="" if count else "\n",
        )
    outputs = [network(binary) for binary in [args.node, args.mite]]
    assert (
        outputs[0] == outputs[1]
        and outputs[1].count("PASS ") == 5
        and outputs[1].endswith("DONE Fetch data network\n")
    ), outputs
    print(outputs[1], end="")
    differences = [network(binary, replay_policy=True) for binary in [args.node, args.mite]]
    assert differences == [
        "multipart replay boundary false\nDONE multipart replay\n",
        "multipart replay boundary true\nDONE multipart replay\n",
    ], differences
    print("PASS explicit Node multipart replay boundary defect and immutable Mite replay")


if __name__ == "__main__":
    main()
