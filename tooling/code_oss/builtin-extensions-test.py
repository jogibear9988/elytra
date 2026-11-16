#!/usr/bin/env python3
"""Small resource-publication controls; no build, installation or real-stage writes.

Pass the original pinned source/archive to check the genuine 60-tree transaction.
All destinations belong to one temporary directory; no production fixture is faked.
"""

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location(
    "builtin_extensions", Path(__file__).with_name("builtin-extensions.py")
)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def rejects(callback):
    try:
        callback()
    except (ValueError, OSError, __import__("subprocess").CalledProcessError):
        return
    raise AssertionError("Expected refusal")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--static-source", type=Path, required=True)
    options = parser.parse_args()
    inventory = tool.archive_inventory(options.archive)
    with tempfile.TemporaryDirectory(prefix="elytra-builtin-controls-") as temporary:
        root = Path(temporary).resolve()
        stage = root / "stage"
        stage.mkdir()
        core = {
            "schema": 1,
            "kind": "diagnostic-source-tree",
            "compiler": "esbuild 0.25.12",
            "inputs": [],
            "adaptations": {"revision": tool.PIN["revision"]},
        }
        core_file = stage / "ELYTRA-COMPILE.json"
        core_file.write_text(json.dumps(core))
        baseline = core_file.read_bytes()
        args = SimpleNamespace(
            static_source=options.static_source.resolve(),
            stage=stage,
            action="stage-static",
            include_copilot=False,
            node=shutil.which("node"),
        )
        assert tool.verify_stage(args)["resources"] is None
        (stage / "extensions").mkdir()
        rejects(lambda: tool.verify_stage(args))
        (stage / "extensions").rmdir()
        tool.static_source(args, inventory)
        report = tool.verify_stage(args)["resources"]
        assert (report["extensionCount"], report["files"], report["bytes"]) == (60, 558, 7673634)
        assert core_file.read_bytes() == baseline
        finder = stage / "extensions/.DS_Store"
        finder.write_bytes(b"owned test Finder metadata")
        tool.static_source(args, inventory)  # Verified idempotence, no replacement.
        assert finder.read_bytes() == b"owned test Finder metadata"
        target = stage / tool.PIN["staticFiles"][0]["path"]
        original = target.read_bytes()
        target.write_bytes(original + b"tampered")
        rejects(lambda: tool.verify_stage(args))
        target.write_bytes(original)
        unknown = stage / "extensions/unknown"
        unknown.mkdir()
        rejects(lambda: tool.verify_stage(args))
        unknown.rmdir()
        partial = stage / ".elytra-builtin-static.pending"
        partial.mkdir()
        rejects(lambda: tool.verify_stage(args))
        partial.rmdir()
        marker = stage / tool.STAGED
        old_marker = marker.read_bytes()
        changed = json.loads(old_marker)
        changed["notStaged"] = []
        marker.write_text(
            json.dumps(tool.sealed({k: v for k, v in changed.items() if k != "seal"}))
        )
        rejects(lambda: tool.verify_stage(args))
        marker.write_bytes(old_marker)
        link = stage / "extensions/escape"
        link.symlink_to(root, target_is_directory=True)
        rejects(lambda: tool.verify_stage(args))
        link.unlink()
        pending, collision = root / "pending", root / "collision"
        pending.mkdir()
        collision.mkdir()
        rejects(lambda: tool.rename_exclusive(pending, collision))
        assert pending.is_dir() and collision.is_dir()
        # A corrupted maintained archive index cannot adopt the extracted source.
        old_hash = tool.PIN["staticFiles"][0]["sha256"]
        tool.PIN["staticFiles"][0]["sha256"] = "0" * 64
        rejects(lambda: tool.static_source(args, inventory))
        tool.PIN["staticFiles"][0]["sha256"] = old_hash
        assert tool.verify_stage(args)["resources"] == report
        assert core_file.read_bytes() == baseline
    print(
        "PASS pinned static resources, atomic collision, ownership, tamper, partial, symlink and Finder controls"
    )


if __name__ == "__main__":
    main()
