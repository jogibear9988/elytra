#!/usr/bin/env python3
"""Require an actual Mite-driven X11 app and independently inspect its live renderer sandbox."""

import _support

import argparse
import http.server
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading

from test_renderer import ROOT, environment, linux_security, stop
from test_x11_geometry import root_property, wait_for


def stage(destination):
    """Bundle the authored ESM preload; native execution uses the actual supported payload."""
    destination.mkdir(parents=True, exist_ok=False)
    source = ROOT / "firefly/tests/linux-runtime"
    for file in source.iterdir():
        shutil.copy2(file, destination / file.name)
    subprocess.run(
        [
            "node",
            str(ROOT / "tooling/bundle_preload.mjs"),
            str(source / "preload.mjs"),
            str(destination / "preload.cjs"),
        ],
        check=True,
    )
    for file in destination.glob("*.mjs"):
        subprocess.run(["node", "--check", str(file)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--check-guard", action="store_true")
    parser.add_argument(
        "--stage-only", type=Path, help="Bundle/check fixture sources without a GUI"
    )
    parser.add_argument(
        "--artifacts-dir", type=Path, default=ROOT / "build/linux-runtime-artifacts"
    )
    args = parser.parse_args()
    if args.stage_only:
        stage(args.stage_only.resolve())
        print("PASS staged modern Linux runtime fixture; no native runtime execution claimed")
        return
    if sys.platform != "linux" or not args.firefly or not args.firefly.is_file():
        parser.error("Build the native Linux Firefly executable before running this gate")
    binary = args.firefly.resolve()
    if args.check_guard:
        base = {key: value for key, value in os.environ.items() if key != "DISPLAY"}
        help_result = subprocess.run(
            [str(binary), "--help"], env=base, capture_output=True, text=True, timeout=10
        )
        assert help_result.returncode == 0 and "Usage: firefly" in help_result.stdout, help_result
        for env in [
            {**base, "QT_QPA_PLATFORM": "xcb"},
            {**base, "QT_QPA_PLATFORM": "wayland", "DISPLAY": ":unreachable"},
        ]:
            result = subprocess.run(
                [str(binary), "unused-main.mjs"],
                env=env,
                capture_output=True,
                text=True,
                timeout=10,
            )
            assert result.returncode == 1 and "requires QT_QPA_PLATFORM=xcb" in result.stderr, (
                result
            )
        print(
            "PASS actual Linux Firefly startup refuses missing display/Wayland before GUI initialization"
        )
        return
    if (
        os.environ.get("QT_QPA_PLATFORM") != "xcb"
        or not os.environ.get("DISPLAY")
        or os.environ.get("FIREFLY_TEST_ISOLATED_X11") != "1"
    ):
        parser.error("Use a dedicated Xvfb display with explicit isolated-X11 opt-in")
    assert not root_property("_NET_SUPPORTING_WM_CHECK"), "Use a fresh X server"
    artifacts = args.artifacts_dir.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    failures = []
    observations = []
    process = None

    class Control(http.server.BaseHTTPRequestHandler):
        def do_POST(self):
            try:
                length = int(self.headers.get("Content-Length", "0"))
                assert self.path == "/" and 0 < length <= 4096, "invalid control request"
                value = json.loads(self.rfile.read(length))
                assert process is not None and process.poll() is None, "main process is not live"
                pid = value["renderer"]
                assert type(pid) is int and pid > 0 and pid != process.pid, value
                renderer = linux_security(pid)
                parent = linux_security(process.pid)
                executable = (Path("/proc") / str(pid) / "exe").resolve(strict=True)
                owned_helper = (binary.parent.parent / "firefly-runtime/bin/WebContent").resolve(
                    strict=True
                )
                assert executable == owned_helper, (executable, owned_helper)
                assert renderer["PPid"] == process.pid, (renderer, process.pid)
                assert renderer["NoNewPrivs"] == 1 and renderer["Seccomp"] == 2, renderer
                assert renderer["Seccomp_filters"] > parent["Seccomp_filters"], (renderer, parent)
                assert value == {
                    "renderer": pid,
                    "ipc": 1,
                    "clipboard": True,
                    "explicitUnsupported": True,
                    "rendered": True,
                }, value
                observations.append(
                    {
                        "app": value,
                        "renderer": renderer,
                        "main": parent,
                        "executable": str(executable),
                    }
                )
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b"VERIFIED")
            except Exception as error:
                failures.append(repr(error))
                self.send_response(500)
                self.end_headers()
                self.wfile.write(b"FAILED")

        def log_message(self, *args):
            pass

    with tempfile.TemporaryDirectory(prefix="firefly-linux-app-") as directory:
        temporary = Path(directory)
        app = temporary / "app"
        stage(app)
        runtime = temporary / "runtime"
        runtime.mkdir(mode=0o700)
        env = {
            **environment(directory),
            "XDG_RUNTIME_DIR": str(runtime),
            "QT_SCALE_FACTOR": "1",
            "QT_SCREEN_SCALE_FACTORS": "1",
        }
        with (artifacts / "window-manager.log").open("wb") as log:
            manager = subprocess.Popen(
                ["openbox", "--sm-disable"],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            try:
                wait_for(lambda: root_property("_NET_SUPPORTING_WM_CHECK"), "Openbox readiness")
                with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Control) as peer:
                    peer.daemon_threads = True
                    thread = threading.Thread(target=peer.serve_forever, daemon=True)
                    thread.start()
                    env["FIREFLY_RUNTIME_CONTROL_URL"] = f"http://127.0.0.1:{peer.server_port}/"
                    process = subprocess.Popen(
                        [str(binary), str(app / "main.mjs")],
                        env=env,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                        start_new_session=True,
                    )
                    try:
                        stdout, stderr = process.communicate(timeout=45)
                    finally:
                        stop(process)
                        stdout, stderr = process.communicate(timeout=5)
                        (artifacts / "stdout.log").write_text(stdout)
                        (artifacts / "stderr.log").write_text(stderr)
                        (artifacts / "kernel.json").write_text(
                            json.dumps(observations, indent=2) + "\n"
                        )
                        peer.shutdown()
                        thread.join(timeout=5)
                assert process.returncode == 0 and not failures and len(observations) == 1, (
                    process.returncode,
                    failures,
                    observations,
                    stdout,
                    stderr,
                )
                values = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
                assert values == [observations[0]["app"]], (values, observations)
                assert manager.poll() is None, "window manager exited during acceptance"
            finally:
                stop(manager)
                manager.wait(timeout=5)
    print(
        "PASS native Linux app lifecycle, HTTP/LibWeb/preload/IPC, clipboard and live kernel sandbox"
    )
    print("Pending: public screen/theme, broader Electron/platform behavior and Linux packaging")


if __name__ == "__main__":
    main()
