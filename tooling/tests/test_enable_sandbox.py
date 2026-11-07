#!/usr/bin/env python3
"""Check public sandbox enforcement and independently inspect the owned live renderer's OS state."""

import _support

import argparse
import ctypes
import json
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import sys
import tempfile
import time
from test_renderer import ROOT, environment, linux_security, run, stop


def native_sandbox(main_pid, helper_pid):
    if sys.platform == "darwin":
        # Query only our own main/helper PIDs. Same null-operation ABI used by
        # Chromium's Seatbelt::IsSandboxed; no private API enters product code.
        library = ctypes.CDLL("/usr/lib/libsandbox.dylib", use_errno=True)
        query = library.sandbox_check
        query.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
        query.restype = ctypes.c_int
        parent = query(main_pid, None, 0)
        renderer = query(helper_pid, None, 0)
        assert parent == 0 and renderer == 1, (parent, renderer, ctypes.get_errno())
        return {"mainSeatbelt": parent, "rendererSeatbelt": renderer}
    if sys.platform.startswith("linux"):
        parent = linux_security(main_pid)
        renderer = linux_security(helper_pid)
        assert renderer["NoNewPrivs"] == 1 and renderer["Seccomp"] == 2, renderer
        assert renderer["Seccomp_filters"] > parent["Seccomp_filters"], (parent, renderer)
        return {"mainKernel": parent, "rendererKernel": renderer}
    raise AssertionError("No native desktop sandbox acceptance for this platform")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node
    with tempfile.TemporaryDirectory(prefix="firefly-enable-sandbox-") as directory:
        root = Path(directory)
        (root / "index.html").write_text("""<!doctype html><title>PENDING</title><script>
const api = globalThis.sandboxFixture;
if (api?.isolated && api.mainMissing && api.processMissing && Object.isFrozen(api)
    && typeof process === 'undefined' && typeof require === 'undefined'
    && typeof __fireflyContextBridge === 'undefined') document.title = 'SANDBOX_OVERRIDE_OK';
</script>""")
        compiler = ROOT / "tooling/runtime-js/node_modules/esbuild/lib/main.js"
        bundle = """import {pathToFileURL} from 'node:url';
const [compiler,source,output]=process.argv.slice(1);
const {buildSync,version}=await import(pathToFileURL(compiler));
if(version!=='0.25.12') throw Error('Wrong preload compiler');
buildSync({entryPoints:[source],outfile:output,bundle:true,platform:'browser',format:'cjs',external:['firefly'],target:'es2022'});
"""
        subprocess.run(
            [
                args.node,
                "--input-type=module",
                "-e",
                bundle,
                str(compiler),
                str(ROOT / "firefly/tests/sandbox/preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
            timeout=20,
        )
        entry = ROOT / "firefly/tests/sandbox/main.mjs"
        default = run(args.firefly.resolve(), "--electron-compat", entry, directory, "default")
        assert (
            default.returncode == 0 and "PASS default sandbox:false rejection" in default.stdout
        ), default
        command = [
            str(args.firefly.resolve()),
            "--electron-compat",
            str(entry),
            directory,
            "enabled",
        ]
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=environment(str(root / "profile")),
            start_new_session=True,
        )
        prefix = ""
        try:
            deadline = time.monotonic() + 30
            helper = None
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                while helper is None:
                    assert time.monotonic() < deadline, ("No helper marker", prefix, process.poll())
                    if not selector.select(timeout=1):
                        continue
                    chunk = os.read(process.stdout.fileno(), 8192)
                    assert chunk, (prefix, process.poll(), process.stderr.read())
                    prefix += chunk.decode("utf-8")
                    for line in prefix.splitlines():
                        if line.startswith('{"sandboxHelper":'):
                            helper = json.loads(line)["sandboxHelper"]
            assert helper and helper != process.pid, helper
            parent = int(
                subprocess.check_output(["ps", "-o", "ppid=", "-p", str(helper)], text=True).strip()
            )
            assert parent == process.pid, (helper, parent, process.pid)
            argv = subprocess.check_output(
                ["ps", "-ww", "-o", "command=", "-p", str(helper)], text=True
            )
            assert "--disable-sandbox" not in argv and "--no-sandbox" not in argv, argv
            state = native_sandbox(process.pid, helper)
            (root / "inspected").write_text("verified")
            stdout, stderr = process.communicate(timeout=15)
            assert (
                process.returncode == 0 and "PASS actual global sandbox override" in prefix + stdout
            ), (prefix + stdout, stderr)
            assert "VERIFICATION FAILED" not in stderr, stderr
            print(default.stdout.strip())
            print((prefix + stdout).strip())
            print(json.dumps(state))
            print("PASS native owned-renderer OS sandbox with sandbox:false globally overridden")
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
