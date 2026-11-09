#!/usr/bin/env python3
"""One hidden session permission batch per requested runtime; all requests are denied before OS access."""

import _support

import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.firefly and not args.electron:
        parser.error("Provide --firefly and/or --electron; no implicit desktop fallback")
    with tempfile.TemporaryDirectory(prefix="elytra-session-permissions-") as temporary:
        for label, binary in [
            ("Firefly offscreen", args.firefly),
            ("Electron hidden reference", args.electron),
        ]:
            if binary is None:
                continue
            root = Path(temporary) / label
            root.mkdir()
            (root / "config").mkdir()
            for name in ["public.mjs", "page.html", "page.mjs"]:
                shutil.copyfile(ROOT / "firefly/tests/session-permissions" / name, root / name)
            if label.startswith("Electron"):
                entry = root / "public.mjs"
                entry.write_text(entry.read_text().replace("from 'firefly'", "from 'electron'"))
            env = environment(str(root / "config"))
            command = [str(binary.resolve()), str(root / "public.mjs")]
            if label.startswith("Firefly"):
                env["QT_QPA_PLATFORM"] = "offscreen"
                env["LIBGL_ALWAYS_SOFTWARE"] = "1"
                command.append("--force-cpu-painting")
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                env=env,
                start_new_session=os.name != "nt",
            )
            try:
                stdout, stderr = process.communicate(timeout=30)
                assert (
                    process.returncode == 0
                    and "PASS public session genuine renderer query" in stdout
                    and "PASS public session actual geolocation" in stdout
                ), (process.returncode, stdout, stderr)
                assert "VERIFICATION FAILED" not in stderr, stderr
                if label.startswith("Firefly"):
                    assert (
                        "PASS public session native handler replacement reset and same-origin contents policy isolation"
                        in stdout
                    ), stdout
                    assert (
                        "PASS public session actual pending requests revoked by navigation owner-close handler replacement and reset"
                        in stdout
                    ), stdout
                print(label + ": " + stdout.strip(), flush=True)
            finally:
                stop(process)
                process.communicate()


if __name__ == "__main__":
    main()
