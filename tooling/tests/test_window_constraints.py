#!/usr/bin/env python3
"""One hidden/offscreen native constraint batch with no desktop fallback."""

import _support

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    parser.add_argument("--platform", choices=["offscreen", "cocoa"], default="offscreen")
    parser.add_argument(
        "--native-probe",
        type=Path,
        help="Exact production Qt objects plus native constraint observer",
    )
    args = parser.parse_args()
    if args.platform == "cocoa" and (sys.platform != "darwin" or args.native_probe):
        parser.error("Cocoa requires macOS and the public Firefly executable only")
    if not args.firefly and not args.electron and not args.native_probe:
        parser.error("Require --firefly, --electron and/or --native-probe")
    with tempfile.TemporaryDirectory(prefix="elytra-constraints-public-") as temporary:
        for label, binary in [
            ("Firefly " + args.platform, args.firefly),
            ("Electron hidden", args.electron),
            ("Firefly native offscreen", args.native_probe),
        ]:
            if binary is None:
                continue
            directory = Path(temporary) / label
            directory.mkdir()
            (directory / "config").mkdir()
            native = label.startswith("Firefly native")
            source = (
                ROOT
                / "firefly/tests/window-constraints"
                / ("native.mjs" if native else "public.mjs")
            ).read_text()
            if label.startswith("Electron"):
                source = source.replace("from 'firefly'", "from 'electron'")
            (directory / "public.mjs").write_text(source)
            env = environment(str(directory / "config"))
            command = [str(binary.resolve()), str(directory / "public.mjs")]
            if label.startswith("Firefly"):
                env["QT_QPA_PLATFORM"] = args.platform
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
                stdout, stderr = process.communicate(timeout=22)
                markers = (
                    [
                        f"PASS native constraints stage {stage} actual QWindow hints and native geometry enforcement"
                        for stage in range(3)
                    ]
                    + [
                        "PASS native constraints constructor setters reset and actual hidden Qt enforcement"
                    ]
                    if native
                    else [
                        "PASS public constraints defaults and fresh native limit snapshots",
                        "PASS public constraints constructor limits clamp real initial outer bounds",
                        "PASS public constraints setters enforce both real outer resize limits",
                        "PASS public constraints content constructor translates frames while setters stay outer",
                        "PASS public constraints independent unbounded axes and limit reset restore resizing",
                        "PASS public constraints minimum wins conflicts remembered maximum and unchanged setter bounds",
                    ]
                )
                assert process.returncode == 0 and all(marker in stdout for marker in markers), (
                    process.returncode,
                    stdout,
                    stderr,
                )
                if label.startswith("Firefly") and not native:
                    assert (
                        "PASS Firefly constraints strict bounded inputs fail before native admission or mutation"
                        in stdout
                    ), (stdout, stderr)
                assert "VERIFICATION FAILED" not in stderr, stderr
                print(label + ": " + stdout.strip(), flush=True)
            finally:
                stop(process)
                process.communicate()


if __name__ == "__main__":
    main()
