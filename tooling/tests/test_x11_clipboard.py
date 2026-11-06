#!/usr/bin/env python3
"""Exercise actual cross-process clipboard ownership only on an explicitly isolated X11 server."""

import _support

import argparse
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import time

from test_renderer import environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    if (
        sys.platform != "linux"
        or not os.environ.get("DISPLAY")
        or os.environ.get("QT_QPA_PLATFORM") != "xcb"
        or os.environ.get("FIREFLY_TEST_ISOLATED_X11") != "1"
    ):
        parser.error("Use the dedicated Xvfb CI display and explicit isolated-X11 environment")
    probe = args.probe.resolve()
    if not probe.is_file():
        parser.error("Build firefly-x11-clipboard-probe first")
    with tempfile.TemporaryDirectory(prefix="firefly-x11-clipboard-") as directory:
        runtime = Path(directory) / "runtime"
        runtime.mkdir(mode=0o700)
        env = {**environment(directory), "XDG_RUNTIME_DIR": str(runtime)}

        def read(mode):
            result = subprocess.run(
                [str(probe), mode], env=env, capture_output=True, text=True, timeout=15
            )
            assert result.returncode == 0 and not result.stderr, result
            assert result.stdout == "PASS native X11 clipboard operation\n", result

        read("local")
        for owner, consumer in [
            ("serve-owned", "read-native"),
            ("serve-external", "read-owned"),
            ("serve-unknown", "read-unknown"),
        ]:
            server = subprocess.Popen(
                [str(probe), owner],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                start_new_session=True,
            )
            try:
                output = b""
                deadline = time.monotonic() + 10
                with selectors.DefaultSelector() as selector:
                    selector.register(server.stdout, selectors.EVENT_READ)
                    while server.poll() is None and time.monotonic() < deadline:
                        if selector.select(0.1):
                            chunk = os.read(server.stdout.fileno(), 65536)
                            if not chunk:
                                break
                            output += chunk
                            if b"READY\n" in output:
                                break
                assert output == b"READY\n" and server.poll() is None, (
                    owner,
                    output,
                    server.poll(),
                )
                read(consumer)
                assert server.poll() is None, "The X11 owner must remain live while serving data"
            finally:
                stop(server)
                stdout, stderr = server.communicate(timeout=5)
                assert not stdout and not stderr, (owner, stdout, stderr)
        print(
            "PASS actual X11 ownership, independent reader/writer, Unicode/NUL/HTML/RTF, refusal and clear"
        )


if __name__ == "__main__":
    main()
