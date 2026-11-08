#!/usr/bin/env python3
"""Run the actual pinned OS-release module with actual platform identity and native files."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from code_oss.overlay import prepare, FILES

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    args = parser.parse_args()
    node = shutil.which("node")
    assert node
    manifest = json.loads((PINNED / "manifest.json").read_text())
    with tempfile.TemporaryDirectory(prefix="elytra-os-release-") as directory:
        work = Path(directory)
        source = work / "source"
        for name in (*FILES, "nls.ts", "osReleaseInfo.ts", "LICENSE.txt"):
            record = manifest["files"][name]
            data = (PINNED / name).read_bytes()
            assert hashlib.sha256(data).hexdigest() == record["sha256"]
            target = source / record["upstream"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        prepare(source, work / "overlay")
        result = subprocess.run(
            [
                node,
                str(ROOT / "tooling/code_oss/compile.mjs"),
                "--source",
                str(source),
                "--overlay",
                str(work / "overlay"),
                "--output",
                str(work / "compiled"),
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )
        assert result.returncode == 0, result
        module = (
            work / "compiled" / manifest["files"]["osReleaseInfo.ts"]["upstream"]
        ).with_suffix(".js")
        driver = work / "driver.mjs"
        driver.write_text(
            """const {getOSReleaseInfo}=await import(MODULE);
const errors=[];const info=await getOSReleaseInfo(error=>errors.push(String(error)));
if(errors.length)throw Error(errors.join(';'));
if(process.platform==='linux'){if(typeof info?.id!=='string'||!info.id)throw Error('No real OS id');}
else if(info!==undefined)throw Error('Unexpected OS release for this native platform');
console.log(JSON.stringify({platform:process.platform,info}));
""".replace("MODULE", json.dumps(module.as_uri()))
        )
        env = dict(os.environ)
        env.pop("VSCODE_NLS_CONFIG", None)
        outputs = []
        for binary in [node, str(args.mite.resolve())]:
            result = subprocess.run(
                [binary, str(driver)], env=env, capture_output=True, text=True, timeout=20
            )
            assert result.returncode == 0 and not result.stderr, result
            outputs.append(json.loads(result.stdout))
        assert outputs[0] == outputs[1], outputs
    if outputs[0]["platform"] == "linux":
        print("PASS actual Code-OSS native Linux OS-release file branch on Node/Mite")
    else:
        print(
            "PASS actual Code-OSS native platform early-return branch; Linux file branch not executed"
        )


if __name__ == "__main__":
    main()
