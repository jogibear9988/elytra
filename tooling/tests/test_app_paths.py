#!/usr/bin/env python3
"""Prove selected profile persistence across real launches and inspect owned native state."""

import _support

import argparse
import http.server
import json
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import threading
import time
from test_renderer import ROOT, environment, run, stop
from test_enable_sandbox import native_sandbox

PAGE = b"""<!doctype html><title>PENDING</title><script>
try {
  const stage = new URL(location.href).searchParams.get('stage');
  if (typeof process !== 'undefined' || typeof require !== 'undefined') throw Error('page privileges');
  if (navigator.languages[0] !== 'de-DE') throw Error('early language');
  const theme = new URL(location.href).searchParams.get('theme');
  if (theme !== 'dark' && theme !== 'unsupported') throw Error('missing native theme contract');
  if (theme === 'dark' && !matchMedia('(prefers-color-scheme: dark)').matches) throw Error('early native theme');
  if (stage === 'write') {
    localStorage.setItem('elytra-persistence', 'actual-native-storage');
    document.cookie = 'elytraPersistence=actual-native-cookie; Max-Age=600; Path=/';
  }
  if (localStorage.getItem('elytra-persistence') !== 'actual-native-storage') throw Error('storage');
  if (!document.cookie.includes('elytraPersistence=actual-native-cookie')) throw Error('cookie');
  document.title = 'PERSIST_OK';
} catch (error) { document.title = 'PERSIST_ERROR ' + error; }
</script>"""


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(PAGE)

    def log_message(self, *args):
        pass


def launch(binary, root, origin, stage):
    process = subprocess.Popen(
        [str(binary), str(ROOT / "firefly/tests/app-paths/main.mjs"), str(root), stage, origin],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=environment(str(root / "native-environment")),
        start_new_session=True,
    )
    prefix = ""
    try:
        deadline = time.monotonic() + 35
        helper = None
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            while helper is None:
                assert time.monotonic() < deadline, (
                    "No persistence marker",
                    stage,
                    prefix,
                    process.poll(),
                )
                if not selector.select(timeout=1):
                    continue
                chunk = os.read(process.stdout.fileno(), 8192)
                assert chunk, (stage, prefix, process.poll(), process.stderr.read())
                prefix += chunk.decode("utf-8")
                for line in prefix.splitlines():
                    if line.startswith('{"pathsHelper":'):
                        helper = json.loads(line)["pathsHelper"]
        parent = int(
            subprocess.check_output(["ps", "-o", "ppid=", "-p", str(helper)], text=True).strip()
        )
        assert parent == process.pid, (helper, parent, process.pid)
        state = native_sandbox(process.pid, helper)
        assert (root / "session/config/Settings.json").is_file(), list(
            (root / "session").rglob("*")
        )
        assert (root / "application/app-setting.txt").read_text() == "persistent app file"
        (root / f"inspected-{stage}").write_text("verified")
        stdout, stderr = process.communicate(timeout=20)
        assert process.returncode == 0 and f"PASS actual {stage}" in prefix + stdout, (
            prefix + stdout,
            stderr,
        )
        assert "VERIFICATION FAILED" not in stderr, stderr
        if sys.platform != "darwin":
            assert "PASS native theme explicitly unsupported; profile checks retained" in prefix + stdout
        print((prefix + stdout).strip())
        print(json.dumps(state))
    finally:
        stop(process)
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    with (
        http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server,
        tempfile.TemporaryDirectory(prefix="firefly-app-paths-") as directory,
    ):
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            root = Path(directory)
            (root / "application").mkdir()
            (root / "session").mkdir()
            broken = root / "broken-profile"
            broken.mkdir()
            (broken / "config").write_text("existing user file must survive")
            failed = run(
                args.firefly.resolve(), ROOT / "firefly/tests/app-paths/failure.mjs", broken
            )
            assert failed.returncode == 1 and "UNEXPECTED_PROFILE_READY" not in failed.stdout, (
                failed
            )
            assert (
                "profile initialization failed" in failed.stderr
                and (broken / "config").read_text() == "existing user file must survive"
            ), failed
            origin = f"http://127.0.0.1:{server.server_port}"
            for stage in ("write", "read"):
                launch(args.firefly.resolve(), root, origin, stage)
            # Only Cocoa has an implemented nativeTheme backend. Other platforms
            # assert its explicit rejection inside every full persistence launch.
            if sys.platform == "darwin":
                preferences = root / "preferences"
                preferences.mkdir()
                for source in ("dark", "system"):
                    result = run(
                        args.firefly.resolve(),
                        ROOT / "firefly/tests/app-paths/preferences.mjs",
                        preferences,
                        source,
                    )
                    assert result.returncode == 0, result
                    value = json.loads(result.stdout.strip())
                    assert (
                        value["themeSource"] == source
                        and value["appearance"]["interfaceTheme"] == source
                        and value["appearance"]["websiteColorScheme"] == source
                    ), value
            databases = [
                file
                for file in (root / "session/data").rglob("*")
                if file.is_file() and file.read_bytes()[:16] == b"SQLite format 3\0"
            ]
            assert databases, list((root / "session").rglob("*"))
            assert not (root / "application/data").exists(), (
                "Browser storage leaked into application userData despite explicit sessionData"
            )
            print(
                "PASS real selected profile SQLite files and cookie/localStorage survival across two launches"
            )
        finally:
            server.shutdown()
            thread.join(timeout=5)


if __name__ == "__main__":
    main()
