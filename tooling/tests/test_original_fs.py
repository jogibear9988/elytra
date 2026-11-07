#!/usr/bin/env python3
"""Exercise physical raw FS, separate facades and host-only resolution without a window."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from test_renderer import stop

ROOT = Path(__file__).resolve().parents[2]


def execute(binary, arguments, environment=None):
    process = subprocess.Popen(
        [str(binary), *map(str, arguments)],
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=os.name != "nt",
    )
    try:
        stdout, stderr = process.communicate(timeout=30)
        return subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr)
    except subprocess.TimeoutExpired as error:
        raise AssertionError(
            ("Raw FS timeout", (error.stdout or b"")[-2000:], (error.stderr or b"")[-3000:])
        ) from None
    finally:
        stop(process)
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.host and not args.firefly:
        parser.error("A built embedded host or Firefly executable is required")
    with tempfile.TemporaryDirectory(prefix="firefly raw-fs %# Grüße ") as directory:
        root = Path(directory).resolve()
        app = root / "app"
        app.mkdir()
        entry = app / "main.mjs"
        shutil.copyfile(ROOT / "firefly/tests/original-fs/main.mjs", entry)
        package = app / "node_modules/original-fs"
        package.mkdir(parents=True)
        (package / "package.json").write_text('{"type":"module","exports":"./index.mjs"}')
        (package / "index.mjs").write_text('export default { kind:"LOCAL_PACKAGE" };\n')
        data = bytes((index * 37 + 11) & 255 for index in range(4096))
        file = root / "physical.bin"
        digest = hashlib.sha256(data).hexdigest()
        runs = []
        if args.host:
            runs.append((args.host.resolve(), [entry, file], None, True))
        if args.firefly:
            # The minimal SDK ships Cocoa/XCB, not Qt's offscreen plugin. Exercise
            # the actual native starter; the fixture never constructs a window.
            environment = {
                **os.environ,
                "QT_QPA_PLATFORM": "cocoa" if sys.platform == "darwin" else "xcb",
            }
            runs.append((args.firefly.resolve(), [entry, file, "--firefly"], environment, False))
        if args.electron:
            environment = {**os.environ, "ELECTRON_RUN_AS_NODE": "1"}
            runs.append((args.electron.resolve(), ["--expose-gc", entry, file], environment, True))
        for binary, arguments, environment, collected in runs:
            file.write_bytes(data)
            Path(str(file) + ".asar").write_bytes(data)
            result = execute(binary, arguments, environment)
            assert result.returncode == 0, (
                result.returncode,
                result.stdout[-2000:],
                result.stderr[:600],
                result.stderr[-2400:],
            )
            if args.host and binary == args.host.resolve():
                assert result.stderr == "HOST_SHUTDOWN\n", result
            elif args.electron and binary == args.electron.resolve():
                assert not result.stderr, result
            report = json.loads(result.stdout)
            assert report == {
                "bytes": 4096,
                "sha256": digest,
                "raw": True,
                "descriptors": True,
                "patchIsolation": True,
                "collected": collected,
            }, report
            assert (
                file.read_bytes() == data
                and Path(str(file) + ".written").read_bytes() == b"first-second"
            )
            print(
                f"PASS {binary.name}: actual raw FS/bytes/hash, descriptor interoperability and patch isolation"
            )
        if args.mite:
            local = app / "local.mjs"
            local.write_text("import fs from 'original-fs'; console.log(fs.kind);\n")
            result = execute(args.mite.resolve(), [local])
            assert (
                result.returncode == 0 and result.stdout == "LOCAL_PACKAGE\n" and not result.stderr
            ), result
            isolated = root / "no-host"
            isolated.mkdir()
            negative = isolated / "entry.mjs"
            negative.write_text(
                "try { await import('original-fs'); throw Error('Unexpected host module'); } catch(e) { console.log(e.code); }\n"
            )
            result = execute(args.mite.resolve(), [negative])
            assert (
                result.returncode == 0
                and result.stdout == "ERR_MODULE_NOT_FOUND\n"
                and not result.stderr
            ), result
            print(
                "PASS standalone Mite has no desktop raw FS module; ordinary npm resolution remains"
            )


if __name__ == "__main__":
    main()
