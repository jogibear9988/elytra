#!/usr/bin/env python3
"""The Code-OSS overlay must preserve the original eager native Worker consumer."""

import _support
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
from code_oss.overlay import prepare

ROOT = Path(__file__).resolve().parents[2]
SOURCE = "diffComputeService.ts"
UPSTREAM = "src/vs/platform/agentHost/node/diffComputeService.ts"
HELPER = "src/vs/base/common/elytraDiffWorker.ts"
SHA256 = "d33579abe20a405fdc4737d3e7ef2367eadbc41b7d5854b5fe30f77a78bf74de"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    # Preserve existing runner arguments; this check is explicitly source-only.
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--source", type=Path)
    args = parser.parse_args()
    pin = ROOT / "vendor/code-oss-tests"
    manifest = json.loads((pin / "manifest.json").read_text())
    record = manifest["files"][SOURCE]
    original = (pin / SOURCE).read_bytes()
    assert record["upstream"] == UPSTREAM
    assert hashlib.sha256(original).hexdigest() == record["sha256"] == SHA256
    assert original.count(b"import { Worker } from 'worker_threads';") == 1
    assert (
        original.count(b"const w = new Worker(workerPath, { name: 'Diff compute worker' });") == 1
    )
    assert b"requireAgentDiffWorker" not in original
    with tempfile.TemporaryDirectory(prefix="elytra-original-diff-worker-") as directory:
        work = Path(directory)
        source = args.source
        if source is None:
            source = work / "source"
            for name, entry in manifest["files"].items():
                target = source / entry["upstream"]
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((pin / name).read_bytes())
        assert (source / UPSTREAM).read_bytes() == original
        before = (source / UPSTREAM).read_bytes()
        output = work / "overlay"
        report = prepare(source, output, libjs_cache=True, libjs_engine=True, local_desktop=True)
        assert (source / UPSTREAM).read_bytes() == before == original
        paths = {entry["path"] for entry in report["files"]}
        assert UPSTREAM not in paths and HELPER not in paths
        assert not (output / UPSTREAM).exists()
        assert not (output / HELPER).exists()
        assert not (ROOT / "firefly/compat/code-oss/diff-worker.ts").exists()
        assert not (ROOT / "tooling/code_oss/diff_worker.py").exists()
        assert "agent worker-backed diff attempts explicitly fail" not in json.dumps(report)
    print(
        "PASS original pinned eager Worker import/constructor preserved; no diff deferral overlay"
    )


if __name__ == "__main__":
    main()
