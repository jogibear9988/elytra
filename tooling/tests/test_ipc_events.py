#!/usr/bin/env python3
"""Batch public one-way IPC acceptance in one hidden window with document reloads."""

import _support
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    parser.add_argument(
        "--electron",
        type=Path,
        help="Run only the shared positive contract in Electron; Firefly-specific limits use the default gate",
    )
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-ipc-events-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/ipc/events", root, dirs_exist_ok=True)
        if args.electron:
            (root / "main.mjs").write_text(
                (root / "main.mjs").read_text().replace("from 'firefly'", "from 'electron'")
            )
            shutil.copyfile(root / "reference-preload.mjs", root / "preload.mjs")
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        (root / "page.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>PENDING</title><script type="module" src="page.mjs"></script>'
        )
        (root / "page.mjs").write_text(
            "try { await desktop.run(); "
            + ("" if args.electron else "await desktop.burst(); ")
            + "document.title='IPC_EVENTS_OK'; } catch(error) { document.title='ERROR:'+error; }"
        )
        (root / "close.html").write_text(
            "<!doctype html><script type=\"module\">history.replaceState(null, '', '#event-source'); desktop.close()</script>"
        )
        result = (
            run(
                args.electron.resolve(),
                "--user-data-dir=" + str(root / "reference-profile"),
                root / "main.mjs",
                "--event-reference",
            )
            if args.electron
            else run(args.firefly.resolve(), root / "main.mjs")
        )
        assert result.returncode == 0, result
        expected = "PASS Electron IPC send:" if args.electron else "PASS public IPC send:"
        assert expected in result.stdout, result
        assert (
            "ERR_FIREFLY_IPC_SEND" not in result.stderr
            and "VERIFICATION FAILED" not in result.stderr
        ), result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
