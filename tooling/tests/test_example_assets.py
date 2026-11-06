#!/usr/bin/env python3
"""Probe example public-asset routing through an independent HTTP peer."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import http.client
import json
from pathlib import Path
import queue
import subprocess
import threading
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]


def probe(binary):
    process = subprocess.Popen(
        [str(binary), str(ROOT / "tooling/fixtures/electron-example-assets.mjs")],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    messages = queue.Queue()
    threading.Thread(target=lambda: messages.put(process.stdout.readline()), daemon=True).start()
    try:
        info = json.loads(messages.get(timeout=15))
        url = urlsplit(info["url"])
        cases = [
            ("GET", url.path, None, 200),
            ("HEAD", url.path, None, 200),
            ("GET", "/shared/style.css", None, 200),
            ("GET", "/quick-start/renderer.mjs", None, 200),
            ("POST", url.path, None, 405),
            ("GET", url.path, "attacker.invalid", 400),
        ]
        for path in [
            "/quick-start/main.mjs",
            "/quick-start/preload.cjs",
            "/runtime.json",
            "/shared/run.mjs",
            "/quick-start/utility.mjs",
            "/catalog.json",
            "/quick-start/../main.mjs",
            "/%2e%2e/runtime.json",
            url.path + "?config=1",
            "/quick-start/missing.html",
        ]:
            cases.append(("GET", path, None, 404))
        for method, path, host, status in cases:
            connection = http.client.HTTPConnection(url.hostname, url.port, timeout=5)
            try:
                connection.request(method, path, headers={"Host": host} if host else {})
                response = connection.getresponse()
                body = response.read()
                assert response.status == status, (binary, method, path, response.status, body)
                if method == "HEAD":
                    assert body == b"" and int(response.getheader("Content-Length")) > 0
                if status == 200:
                    assert response.getheader("Cache-Control") == "no-store"
                    expected = (
                        "text/css"
                        if path.endswith(".css")
                        else "text/javascript"
                        if path.endswith(".mjs")
                        else "text/html"
                    )
                    assert response.getheader("Content-Type").startswith(expected)
            finally:
                connection.close()
        print("PASS asset allowlist, methods, Host validation, MIME and HEAD:", binary, flush=True)
    finally:
        process.terminate()
        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    args = parser.parse_args()
    probe("node")
    probe(args.mite.resolve())


if __name__ == "__main__":
    main()
