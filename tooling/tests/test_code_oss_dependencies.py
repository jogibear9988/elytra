#!/usr/bin/env python3
"""Check the actual pinned root graph and Code-OSS CLI parser without network or GUI."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node
    manifest = json.loads((PINNED / "manifest.json").read_text())
    with tempfile.TemporaryDirectory(prefix="elytra-code-oss-dependencies-") as directory:
        root = Path(directory).resolve()
        source = root / "source"
        for name, entry in manifest["files"].items():
            data = (PINNED / name).read_bytes()
            assert hashlib.sha256(data).hexdigest() == entry["sha256"], name
            target = source / entry["upstream"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        output = root / "prepared"
        tool = ROOT / "tooling/code_oss/dependencies.mjs"
        command = [args.node, str(tool), "--source", str(source), "--inventory-only"]
        result = subprocess.run(
            [*command, "--output", str(output)], capture_output=True, text=True, timeout=20
        )
        assert result.returncode == 0 and not result.stderr, result
        report = json.loads((output / "ELYTRA-DEPENDENCIES.json").read_text())
        assert report["state"] == "prepared-not-installed" and report["lifecycleScripts"] is False
        assert not (output / "node_modules").exists()
        assert len(report["graph"]["roots"]) == 71 and len(report["graph"]["packages"]) == 258
        roots = {item["name"]: item for item in report["graph"]["roots"]}
        assert roots["minimist"]["version"] == "1.2.8"
        assert {"node-pty", "@parcel/watcher", "@vscode/sqlite3", "native-keymap"} <= roots.keys()
        assert "typescript" not in roots, "Do not silently install root devDependencies"
        # Hosted native installs use the checked-in flat corpus, without a full source checkout.
        flat_output = root / "flat-prepared"
        flat = subprocess.run(
            [args.node, str(tool), "--inventory-only", "--output", str(flat_output)],
            capture_output=True,
            text=True,
            timeout=20,
        )
        assert flat.returncode == 0 and not flat.stderr, flat
        flat_report = json.loads((flat_output / "ELYTRA-DEPENDENCIES.json").read_text())
        assert flat_report["graph"] == report["graph"]
        assert flat_report["sourceInputs"] == report["sourceInputs"]
        for name, entry in manifest["files"].items():
            assert (source / entry["upstream"]).read_bytes() == (PINNED / name).read_bytes()
        (source / "package.json").write_bytes((source / "package.json").read_bytes() + b"\n")
        refused = root / "refused"
        bad = subprocess.run(
            [*command, "--output", str(refused)], capture_output=True, text=True, timeout=20
        )
        assert (
            bad.returncode != 0
            and "Pinned dependency input differs" in bad.stderr
            and not refused.exists()
        ), bad
        (source / "package.json").write_bytes((PINNED / "package.json").read_bytes())
        incomplete = root / "incomplete-build"
        rejected = subprocess.run(
            [
                args.node,
                str(ROOT / "tooling/code_oss/compile.mjs"),
                "--source",
                str(source),
                "--dependencies",
                str(output),
                "--output",
                str(incomplete),
            ],
            capture_output=True,
            text=True,
            timeout=20,
        )
        assert (
            rejected.returncode != 0
            and "Dependency tree is unaccepted" in rejected.stderr
            and not incomplete.exists()
        ), rejected
        # The NPM compatibility fixture is the exact version/integrity selected by Code-OSS.
        lock = json.loads((ROOT / "tooling/npm-fixtures/package-lock.json").read_text())
        selected = next(
            item for item in report["graph"]["packages"] if item["snapshot"] == "minimist@1.2.8"
        )
        assert lock["packages"]["node_modules/minimist"]["integrity"] == selected["integrity"]
        dependency = ROOT / "tooling/npm-fixtures/node_modules/minimist"
        assert json.loads((dependency / "package.json").read_text())["version"] == "1.2.8"
        extracted = root / "cli.mjs"
        extract = subprocess.run(
            [
                args.node,
                str(ROOT / "tooling/code_oss/extract-cli.mjs"),
                str(PINNED / "mainImpl.ts"),
                str(dependency / "index.js"),
                str(extracted),
            ],
            capture_output=True,
            text=True,
            timeout=20,
        )
        assert extract.returncode == 0 and not extract.stderr, extract
        shutil.copyfile(PINNED / "LICENSE.txt", root / "LICENSE.txt")
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("VSCODE_", "ELECTRON_"))
        }
        results = [
            subprocess.run(
                [
                    str(binary),
                    str(ROOT / "mite/tests/process-events/code-oss-cli.mjs"),
                    str(extracted),
                ],
                env=env,
                capture_output=True,
                text=True,
                timeout=20,
            )
            for binary in [args.node, args.mite.resolve()]
        ]
        assert all(item.returncode == 0 and not item.stderr for item in results), results
        assert results[0].stdout == results[1].stdout, results
        rows = [json.loads(line) for line in results[0].stdout.splitlines()]
        assert len(rows) == 14 and rows[0]["sandbox"] is True and rows[5]["sandbox"] is False
        assert rows[3]["user-data-dir"] == "/tmp/a b # Grüße"
        print(
            "PASS real frozen Code-OSS root graph, pre-mutation drift refusal and exact npm dependency integrity"
        )
        print(
            "PASS actual pinned Code-OSS CLI subroutine/minimist in Node and Mite: 14 cases, no desktop simulation"
        )


if __name__ == "__main__":
    main()
