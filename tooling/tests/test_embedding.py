#!/usr/bin/env python3
"""Exercise external-loop Mite hosting and CLI behavior through the shared runtime."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def execute(binary, arguments, **kwargs):
    return subprocess.run(
        [str(binary), *arguments], text=True, capture_output=True, timeout=15, **kwargs
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    cases = [
        ("console.log(process.versions.libjs, Buffer.from('mite').toString('hex'))", 0, None),
        (
            "const a=[]; process.nextTick(()=>a.push('tick')); Promise.resolve().then(()=>a.push('promise')); setTimeout(()=>console.log(JSON.stringify(a)),5)",
            0,
            '["tick","promise"]\n',
        ),
        (
            "const {Readable}=require('node:stream'); const a=[]; Readable.from(['a','b']).on('data',x=>a.push(x)).on('end',()=>console.log(a.join('')))",
            0,
            "ab\n",
        ),
        ("setTimeout(()=>{throw Error('embedded timer failure')},5)", 1, ""),
        ("process.exitCode=7; console.log('explicit exit code')", 7, "explicit exit code\n"),
    ]
    with tempfile.TemporaryDirectory(prefix="mite-embedding-cjs-") as directory:
        script = Path(directory) / "entry.cjs"
        for source, status, stdout in cases:
            script.write_text(source)
            cli = execute(args.mite, [str(script)])
            host = execute(args.host, [str(script)])
            assert host.stderr.count("HOST_SHUTDOWN\n") == 1, host
            assert host.returncode == cli.returncode == status and host.stdout == cli.stdout, (
                host,
                cli,
            )
            if stdout is not None:
                assert host.stdout == stdout, host
            assert ("embedded timer failure" in host.stderr) == (
                "embedded timer failure" in cli.stderr
            ), (host, cli)
    print("PASS embedded libuv timers/ticks/promises/streams/error/exit behavior agrees with CLI")
    with tempfile.TemporaryDirectory(prefix="mite-embedding-") as directory:
        root = Path(directory)
        file = root / "data.txt"
        file.write_text("asynchronous filesystem")
        script = root / "entry.mjs"
        script.write_text(
            "import {readFile} from 'node:fs/promises'; const value = await readFile("
            + json.dumps(str(file))
            + ", 'utf8'); console.log(value)"
        )
        cli = execute(args.mite, [str(script)])
        host = execute(args.host, [str(script)])
        assert (
            host.returncode == cli.returncode == 0
            and host.stdout == cli.stdout == "asynchronous filesystem\n"
        ), (host, cli)
        script.write_text("""
const log = [];
const value = await new Promise(resolve => embeddedHost.onPulse(() => {
  log.push('host');
  process.nextTick(() => log.push('tick'));
  Promise.resolve().then(() => log.push('promise'));
  resolve('settled');
}));
log.push(value);
setTimeout(() => console.log(JSON.stringify(log)), 2);
""")
        host = execute(args.host, [str(script)])
        assert host.returncode == 0 and json.loads(host.stdout) == [
            "host",
            "tick",
            "promise",
            "settled",
        ], host
        assert host.stderr == "HOST_SHUTDOWN\n", host
        if hasattr(os, "mkfifo"):
            fifo = root / "blocked"
            os.mkfifo(fifo)
            source = (
                "require('node:fs').readFile("
                + json.dumps(str(fifo))
                + ",()=>{}); embeddedHost.onPulse(()=>{throw Error('host blocked worker failure')})"
            )
            blocked_script = root / "blocked.cjs"
            blocked_script.write_text(source)
            blocked = execute(args.host, [str(blocked_script)])
            assert (
                blocked.returncode == 1
                and "host blocked worker failure" in blocked.stderr
                and blocked.stderr.count("HOST_SHUTDOWN\n") == 1
            ), blocked
            failed_work = execute(
                args.host,
                ["-e", "console.log('must not execute')"],
                env={**os.environ, "MITE_EMBED_FAIL_INIT_WORK_PATH": str(fifo)},
            )
            assert (
                failed_work.returncode == 1
                and failed_work.stdout == ""
                and "Embedded host failed with pending work" in failed_work.stderr
                and failed_work.stderr.count("HOST_SHUTDOWN\n") == 1
            ), failed_work
    reentry = execute(
        args.host,
        [
            "-e",
            "const log=[];embeddedHost.onPulse(()=>{process.nextTick(()=>log.push('tick'));Promise.resolve().then(()=>log.push('promise'));if(embeddedHost.reenterPump()!==0)throw Error('nested pump status');log.push('host');setTimeout(()=>console.log(JSON.stringify(log)),2);});",
        ],
    )
    assert reentry.returncode == 0 and json.loads(reentry.stdout) == ["host", "tick", "promise"], (
        reentry
    )
    print("PASS ESM/fs completion and host-owned liveness with rooted callbacks and checkpoints")
    fatal = execute(
        args.host, ["-e", "embeddedHost.onPulse(()=>{throw Error('host callback failure')})"]
    )
    assert (
        fatal.returncode == 1
        and "host callback failure" in fatal.stderr
        and fatal.stderr.count("HOST_SHUTDOWN\n") == 1
    ), fatal
    rejected = execute(
        args.host, ["-e", "embeddedHost.onPulse(()=>Promise.reject(Error('host rejection')))"]
    )
    assert (
        rejected.returncode == 1
        and "host rejection" in rejected.stderr
        and rejected.stderr.count("HOST_SHUTDOWN\n") == 1
    ), rejected
    failed = execute(
        args.host,
        ["-e", "console.log('must not execute')"],
        env={**os.environ, "MITE_EMBED_FAIL_INIT": "1"},
    )
    assert (
        failed.returncode == 1
        and failed.stdout == ""
        and "Embedded host initialization failed" in failed.stderr
        and failed.stderr.count("HOST_SHUTDOWN\n") == 1
    ), failed
    print("PASS failed initialization, fatal host callbacks/rejections and exactly-once shutdown")


if __name__ == "__main__":
    main()
