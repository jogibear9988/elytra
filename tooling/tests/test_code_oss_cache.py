#!/usr/bin/env python3
"""Run real Code-OSS cache branches through the explicit LibJS application policy."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from code_oss.overlay import adapt_cache, prepare

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node
    manifest = json.loads((PINNED / "manifest.json").read_text())
    with tempfile.TemporaryDirectory(prefix="elytra-code-oss-cache-") as directory:
        work = Path(directory).resolve()
        source = work / "original"
        for name, entry in manifest["files"].items():
            data = (PINNED / name).read_bytes()
            assert hashlib.sha256(data).hexdigest() == entry["sha256"], name
            target = source / entry["upstream"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        original = (PINNED / "nodeCompileCache.ts").read_bytes()
        for seam in [
            "from 'node:v8'",
            "export function enableNodeCompileCache",
            "export async function waitForNodeCompileCacheReady",
        ]:
            try:
                adapt_cache(original.replace(seam.encode(), b"changed"))
            except ValueError:
                pass
            else:
                raise AssertionError("Cache adaptation accepted seam drift")
        overlay = work / "overlay"
        report = prepare(source, overlay, libjs_cache=True)
        assert len(report["adaptations"]) == 2
        compile_tool = ROOT / "tooling/code_oss/compile.mjs"
        trees = {"original": work / "original-js", "adapted": work / "adapted-js"}
        for name, tree in trees.items():
            options = ["--overlay", str(overlay)] if name == "adapted" else []
            result = subprocess.run(
                [
                    args.node,
                    str(compile_tool),
                    "--source",
                    str(source),
                    "--output",
                    str(tree),
                    *options,
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
            assert result.returncode == 0 and not result.stderr, result
            compiled = json.loads((tree / "ELYTRA-COMPILE.json").read_text())
            assert compiled["kind"] == "diagnostic-source-tree"
            assert (tree / "LICENSE.txt").read_bytes() == (source / "LICENSE.txt").read_bytes()
            assert (tree / "ELYTRA-LICENSE.txt").read_bytes() == (ROOT / "LICENSE").read_bytes()
        clean = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("VSCODE_", "ELECTRON_"))
        }
        for mode in ["development", "missing", "disabled", "generation", "measurements"]:
            results = []
            pairs = (
                [(args.node, "original"), (args.node, "adapted"), (args.mite.resolve(), "adapted")]
                if mode in ["development", "missing"]
                else [(args.node, "adapted"), (args.mite.resolve(), "adapted")]
            )
            for index, (binary, name) in enumerate(pairs):
                env = dict(
                    clean, VSCODE_NODE_COMPILE_CACHE_ROOT=str(work / f"cache-{mode}-{index}")
                )
                if mode == "development":
                    env["VSCODE_DEV"] = "1"
                if mode == "generation":
                    env.update(VSCODE_DEV="1", VSCODE_GENERATE_NODE_COMPILE_CACHE="1")
                if mode == "measurements":
                    env.update(
                        VSCODE_DEV="1",
                        VSCODE_NODE_COMPILE_CACHE_MEASUREMENTS=str(work / "measurements"),
                    )
                result = subprocess.run(
                    [
                        str(binary),
                        str(ROOT / "mite/tests/process-events/code-oss-cache.mjs"),
                        str(trees[name]),
                        mode,
                    ],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=20,
                )
                assert result.returncode == 0, result
                if mode == "disabled":
                    assert (
                        "status: disabled" in result.stderr and "active: none" in result.stderr
                    ), result
                    assert result.stderr.count("\n") == 1, result
                else:
                    assert not result.stderr, result
                results.append(result.stdout)
            assert len(set(results)) == 1 and results[0].startswith("PASS "), results
            print(results[0], end="", flush=True)
        assert not (work / "measurements").exists(), "No fabricated V8 statistics"
        for name, entry in manifest["files"].items():
            assert (source / entry["upstream"]).read_bytes() == (PINNED / name).read_bytes()
        # Preserve explicit rejection of changed overlay bytes before creating outputs.
        helper = overlay / "src/vs/base/node/elytraCompileCache.ts"
        helper.write_bytes(helper.read_bytes() + b"\n")
        refused = work / "refused"
        result = subprocess.run(
            [
                args.node,
                str(compile_tool),
                "--source",
                str(source),
                "--output",
                str(refused),
                "--overlay",
                str(overlay),
            ],
            capture_output=True,
            text=True,
            timeout=20,
        )
        assert (
            result.returncode != 0
            and "Overlay input/output drift" in result.stderr
            and not refused.exists()
        ), result
        # Console warning primitive is required by the real upstream disabled branch.
        warning = 'console.warn("warning",17,true); console.log("continued");'
        checks = [
            subprocess.run([str(binary), "-e", warning], capture_output=True, text=True, timeout=10)
            for binary in [args.node, args.mite.resolve()]
        ]
        assert all(
            result.returncode == 0
            and result.stdout == "continued\n"
            and result.stderr == "warning 17 true\n"
            for result in checks
        ), checks
        print(
            "PASS preserved original sources, overlay drift refusal and native console.warn routing"
        )


if __name__ == "__main__":
    main()
