#!/usr/bin/env python3
"""Run hidden public spellcheck admission and exact-object native provider checks."""

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
    parser.add_argument("--native-probe", type=Path)
    args = parser.parse_args()
    if not any([args.firefly, args.electron, args.native_probe]):
        parser.error("Require a Firefly, Electron or native probe executable")
    with tempfile.TemporaryDirectory(prefix="elytra-spellcheck-") as temporary:
        for label, binary, script in [
            ("Firefly offscreen", args.firefly, "public.mjs"),
            ("Electron hidden", args.electron, "public.mjs"),
            ("Native provider offscreen", args.native_probe, "native.mjs"),
        ]:
            if binary is None:
                continue
            directory = Path(temporary) / label
            directory.mkdir()
            (directory / "config").mkdir()
            for name in [script, "editor.html"]:
                shutil.copyfile(ROOT / "firefly/tests/spellcheck" / name, directory / name)
            entry = directory / script
            if label.startswith("Electron"):
                entry.write_text(entry.read_text().replace("from 'firefly'", "from 'electron'"))
            env = environment(str(directory / "config"))
            command = [str(binary.resolve()), str(entry)]
            if not label.startswith("Electron"):
                env.update(QT_QPA_PLATFORM="offscreen", LIBGL_ALWAYS_SOFTWARE="1")
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
                assert process.returncode == 0, (label, process.returncode, stdout, stderr)
                assert "VERIFICATION FAILED" not in stderr, stderr
                if script == "public.mjs":
                    assert (
                        "PASS public spellcheck false window and child load real documents while hidden"
                        in stdout
                    ), (stdout, stderr)
                    if label.startswith("Firefly"):
                        assert (
                            "PASS Firefly spellcheck enabled and nonboolean requests reject before allocation"
                            in stdout
                        ), (stdout, stderr)
                else:
                    for marker in [
                        "PASS native spellcheck real provider rejects factories without allocation",
                        "PASS native spellcheck null provider restores hidden window and view admission",
                        "PASS native spellcheck provider-null invariant and restored public admission",
                    ]:
                        assert marker in stdout, (stdout, stderr)
                print(label + ": " + stdout.strip(), flush=True)
            finally:
                stop(process)
                process.communicate()


if __name__ == "__main__":
    main()
