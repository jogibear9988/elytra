#!/usr/bin/env python3
"""Verify exact Code-OSS native identity adaptation and actual upstream module behavior."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from code_oss.overlay import adapt, prepare, FILES, NODE_CHECK

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def compile_sources(node, source, output, paths):
    # Build-only TypeScript transform supports the actual upstream const enums.
    result = subprocess.run(
        [
            node,
            "--input-type=module",
            "-e",
            'import fs from "node:fs"; import path from "node:path"; import {pathToFileURL} from "node:url"; '
            "const [src,dst,list,compiler]=process.argv.slice(1); const {transformSync,version}=await import(pathToFileURL(compiler)); "
            'if(version!=="0.25.12")throw Error("Wrong TypeScript transformer"); for(const name of JSON.parse(list)) {'
            'const out=path.join(dst,name.replace(/\.ts$/,".js")); fs.mkdirSync(path.dirname(out),{recursive:true}); '
            'fs.writeFileSync(out,transformSync(fs.readFileSync(path.join(src,name),"utf8"),{loader:"ts",format:"esm",target:"es2022"}).code);}',
            str(source),
            str(output),
            json.dumps(paths),
            str(ROOT / "tooling/runtime-js/node_modules/esbuild/lib/main.js"),
        ],
        capture_output=True,
        timeout=20,
    )
    assert result.returncode == 0 and not result.stderr, result
    (output / "package.json").write_text('{"type":"module"}\n')


def rejected(callback):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError("Unsafe overlay accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node, "Node development oracle/transformer required"
    manifest = json.loads((PINNED / "manifest.json").read_text())
    with tempfile.TemporaryDirectory(prefix="elytra-code-oss-platform-") as directory:
        work = Path(directory).resolve()
        source = work / "original"
        paths = []
        for name in (*FILES, "nls.ts", "LICENSE.txt"):
            record = manifest["files"][name]
            data = (PINNED / name).read_bytes()
            assert hashlib.sha256(data).hexdigest() == record["sha256"]
            destination = source / record["upstream"]
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            if name.endswith(".ts"):
                paths.append(record["upstream"])
        before = {path: (source / path).read_bytes() for path in paths}
        overlay = work / "overlay"
        report = prepare(source, overlay)
        assert report["kind"] == "partial-source-overlay"
        assert (overlay / "LICENSE.txt").read_bytes() == (source / "LICENSE.txt").read_bytes()
        for name in FILES:
            path = manifest["files"][name]["upstream"]
            patched = (overlay / path).read_text()
            original = before[path].decode()
            expected = original.replace(
                NODE_CHECK,
                "(typeof process?.versions?.node === 'string' || typeof process?.versions?.mite === 'string')",
            )
            if name == "platform.ts":
                expected = expected.replace(
                    "\t\tnode?: string;", "\t\tnode?: string;\n\t\tmite?: string;"
                )
            assert patched == expected
            rejected(lambda: adapt(name, original.replace(NODE_CHECK, "false").encode()))
            rejected(lambda: adapt(name, (original + NODE_CHECK).encode()))
        rejected(lambda: prepare(source, source / "inside"))
        rejected(lambda: prepare(source, overlay))
        damaged = source / manifest["files"][FILES[-1]]["upstream"]
        damaged.write_bytes(damaged.read_bytes() + b"\n")
        rejected(lambda: prepare(source, work / "refused"))
        assert not (work / "refused").exists(), "Refusal must precede output mutation"
        damaged.write_bytes(before[manifest["files"][FILES[-1]]["upstream"]])
        assert all((source / path).read_bytes() == data for path, data in before.items())
        # Git and GitHub archives honor the upstream LICENSE.txt eol=crlf rule.
        crlf_source = work / "crlf-source"
        shutil.copytree(source, crlf_source)
        license_path = crlf_source / "LICENSE.txt"
        license_data = license_path.read_bytes().replace(b"\n", b"\r\n")
        license_path.write_bytes(license_data)
        crlf_output = work / "crlf-overlay"
        prepare(crlf_source, crlf_output)
        assert (crlf_output / "LICENSE.txt").read_bytes() == license_data
        license_path.write_bytes(license_data + b"drift")
        rejected(lambda: prepare(crlf_source, work / "bad-license"))
        assert not (work / "bad-license").exists()
        merged = work / "adapted-source"
        shutil.copytree(source, merged)
        shutil.copytree(overlay, merged, dirs_exist_ok=True)
        original_js, adapted_js = work / "original-js", work / "adapted-js"
        compile_sources(args.node, source, original_js, paths)
        compile_sources(args.node, merged, adapted_js, paths)
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("VSCODE_", "ELECTRON_"))
        }
        env["VSCODE_CWD"] = str(source)
        env["VSCODE_NLS_CONFIG"] = json.dumps(
            {
                "userLocale": "fr-fr",
                "osLocale": "de-de",
                "regionalLocale": "de-at",
                "resolvedLanguage": "fr",
            }
        )
        driver = ROOT / "mite/tests/process-events/code-oss-platform.mjs"
        results = []
        for binary, tree in [
            (args.node, original_js),
            (args.node, adapted_js),
            (args.mite.resolve(), adapted_js),
        ]:
            result = subprocess.run(
                [str(binary), str(driver), str(tree), "native"],
                env=env,
                capture_output=True,
                text=True,
                timeout=20,
            )
            assert result.returncode == 0 and not result.stderr, result
            results.append(result.stdout)
        assert len(set(results)) == 1 and results[0].startswith("PASS actual Code-OSS"), results
        negative = subprocess.run(
            [str(args.mite.resolve()), str(driver), str(original_js), "unadapted"],
            env=env,
            capture_output=True,
            text=True,
            timeout=20,
        )
        assert (
            negative.returncode == 0
            and negative.stdout == "PASS unadapted Code-OSS rejects native Mite classification\n"
        ), negative
        assert "Unable to resolve platform." in negative.stderr, negative
        print(results[0], end="", flush=True)
        print(
            "PASS unadapted negative control, exact source preservation and pre-mutation drift refusal"
        )


if __name__ == "__main__":
    main()
