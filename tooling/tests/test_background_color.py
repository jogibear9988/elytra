#!/usr/bin/env python3
"""One hidden public background-color batch; renderer pixels are a separate native gate."""

import _support

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    parser.add_argument(
        "--native-probe", type=Path, help="Exact production objects plus native pixel observer"
    )
    args = parser.parse_args()
    if not args.firefly and not args.electron and not args.native_probe:
        parser.error("Require --firefly, --electron and/or --native-probe")
    with tempfile.TemporaryDirectory(prefix="elytra-background-public-") as temporary:
        for label, binary in [
            ("Firefly offscreen", args.firefly),
            ("Electron hidden", args.electron),
            ("Firefly native pixels offscreen", args.native_probe),
        ]:
            if binary is None:
                continue
            directory = Path(temporary) / label
            directory.mkdir()
            (directory / "config").mkdir()
            for name in ["public.mjs", "native.mjs", "blank.html", "authored.html"]:
                shutil.copyfile(ROOT / "firefly/tests/background-color" / name, directory / name)
            if label.startswith("Electron"):
                entry = directory / "public.mjs"
                entry.write_text(entry.read_text().replace("from 'firefly'", "from 'electron'"))
            env = environment(str(directory / "config"))
            native = label.startswith("Firefly native")
            command = [
                str(binary.resolve()),
                str(directory / ("native.mjs" if native else "public.mjs")),
            ]
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
                markers = (
                    [f"PASS native background stage {stage} actual " for stage in range(7)]
                    + [
                        "PASS native background blank author override live update navigation and independent child view"
                    ]
                    if native
                    else [
                        "PASS public background constructor native getter survives actual renderer load",
                        "PASS public background opaque CSS colors invalid fallback and strict setter types",
                        "PASS public background setter persists document replacement with window hidden",
                        "PASS public background independent child view setter and genuine public API shape",
                    ]
                )
                assert process.returncode == 0 and all(marker in stdout for marker in markers), (
                    process.returncode,
                    stdout,
                    stderr,
                )
                assert "VERIFICATION FAILED" not in stderr, stderr
                if label.startswith("Firefly") and not native:
                    assert (
                        "PASS Firefly background nonopaque colors explicitly unsupported without state mutation"
                        in stdout
                    ), (stdout, stderr)
                print(label + ": " + stdout.strip(), flush=True)
            finally:
                stop(process)
                process.communicate()


if __name__ == "__main__":
    main()
