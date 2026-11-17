#!/usr/bin/env python3
"""Source-only sample integrity controls, not native package or editor acceptance."""

import _support

import json
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

from build_code_oss import (
    ADAPTERS,
    ENTRY,
    INVENTORY,
    PIN,
    digest,
    inventory,
    prepare_source,
    verify_sample,
)


def rejected(operation):
    try:
        operation()
    except (ValueError, OSError):
        return
    raise AssertionError("Altered/unowned sample accepted")


def fixture(root):
    pin = json.loads(PIN.read_text())
    (root / "src").mkdir(exist_ok=True)
    (root / "main.mjs").write_text(ENTRY)
    (root / "package.json").write_text(json.dumps({"name": "synthetic-code-oss-control"}))
    (root / "src/main.js").write_text("// Synthetic source-control input, not a native app.\n")
    (root / "LICENSE.txt").write_text("fixture notice\n")
    (root / "ELYTRA-COMPILE.json").write_text(
        json.dumps(
            {
                "schema": 1,
                "kind": "diagnostic-source-tree",
                "adaptations": {"revision": pin["revision"]},
                "rendererAssets": {"fixture": True},
                "nls": {"fixture": True},
                "packageAdaptations": [{"fixture": name} for name in ADAPTERS],
            }
        )
    )
    (root / INVENTORY).write_text(json.dumps(inventory(root)))
    metadata = {
        "schema": 1,
        "kind": "firefly-code-oss-sample",
        "runtime": "firefly",
        "electronCompat": True,
        "upstream": {key: pin[key] for key in ("repository", "revision", "archiveSha256")},
        "compileSha256": digest(root / "ELYTRA-COMPILE.json"),
        "inventorySha256": digest(root / INVENTORY),
        "builtinExtensions": "partial-static-only",
        "staticExtensionCount": pin["staticCount"],
        "unbuiltExtensionCount": pin["needsBuildCount"],
        "desktopAcceptance": "not-established-by-build",
    }
    (root / "BUILD.json").write_text(json.dumps(metadata))
    return metadata


def main():
    with tempfile.TemporaryDirectory(prefix="code-oss-sample-control-") as directory:
        root = Path(directory).resolve()
        source = root / "source"
        source.mkdir()
        # A directory nested in another checkout must not lead git -C to fetch
        # or check out the parent, even when its remote would match Code-OSS.
        with (
            patch("build_code_oss.subprocess.check_output", return_value=str(root)) as read,
            patch("build_code_oss.run") as mutation,
        ):
            rejected(lambda: prepare_source(source, json.loads(PIN.read_text())))
            mutation.assert_not_called()
            assert read.call_count == 1
            assert read.call_args.args[0] == ["git", "-C", source, "rev-parse", "--show-toplevel"]
        source.rmdir()
        expected = fixture(root)
        assert verify_sample(root) == expected
        assert "OFFSCREEN" not in ENTRY and "DIAGNOSTIC" not in ENTRY
        for name in ("BUILD.json", INVENTORY, "ELYTRA-COMPILE.json", "package.json", "main.mjs"):
            target = root / name
            original = target.read_bytes()
            # Both an otherwise-valid internal metadata alias and an external
            # alias must fail at the physical-authority check, before JSON reads.
            for alias in (root / "metadata-alias", root.parent / (root.name + "-metadata-alias")):
                alias.write_bytes(original)
                target.unlink()
                # Real native links are required on Windows too; insufficient
                # symlink privileges fail this control rather than skipping it.
                target.symlink_to(alias)
                try:
                    verify_sample(root)
                except ValueError as error:
                    assert str(error) == "Code-OSS sample requires physical metadata: " + name
                else:
                    raise AssertionError("Metadata symlink accepted: " + name)
                finally:
                    target.unlink()
                    target.write_bytes(original)
                    alias.unlink()
            assert verify_sample(root) == expected
        for target in [root / "src/main.js", root / "LICENSE.txt", root / "main.mjs"]:
            original = target.read_bytes()
            target.write_bytes(original + b"// drift\n")
            rejected(lambda: verify_sample(root))
            target.write_bytes(original)
        extra = root / "unowned.js"
        extra.write_text("unexpected")
        rejected(lambda: verify_sample(root))
        extra.unlink()
        source = root / "src/main.js"
        original_mode = source.stat().st_mode
        original_inventory = inventory(root)
        try:
            source.chmod(original_mode | 0o111)
            observed_mode = source.stat().st_mode
            if observed_mode & 0o111 != original_mode & 0o111:
                rejected(lambda: verify_sample(root))
            else:
                # Windows chmod controls the read-only attribute, not executable
                # bits. Prove unchanged native metadata rather than inventing an
                # executable-bit change. PE/execute-ACL acceptance is separate.
                assert os.name == "nt", "POSIX executable-bit change was not applied"
                assert observed_mode == original_mode
                assert inventory(root) == original_inventory
                assert verify_sample(root) == expected
        finally:
            source.chmod(original_mode)
        assert source.stat().st_mode == original_mode
        link = root / "out"
        link.symlink_to("src", target_is_directory=True)
        fixture(root)
        verify_sample(root)
        link.unlink()
        link.symlink_to(root.parent, target_is_directory=True)
        rejected(lambda: inventory(root))
        link.unlink()
        packages = root / "node_modules/.pnpm/example/node_modules/example"
        packages.mkdir(parents=True)
        (packages / "index.js").write_text("// owned dependency fixture\n")
        # Win32 stores a relative symlink target verbatim; use the native path
        # separators rather than placing POSIX slash bytes in its reparse data.
        package_target = Path(".pnpm") / "example" / "node_modules" / "example"
        (root / "node_modules/example").symlink_to(package_target, target_is_directory=True)
        assert os.readlink(root / "node_modules/example") == str(package_target)
        fixture(root)
        verify_sample(root)
        metadata = json.loads((root / "BUILD.json").read_text())
        metadata["desktopAcceptance"] = "accepted"
        (root / "BUILD.json").write_text(json.dumps(metadata))
        rejected(lambda: verify_sample(root))
    print("PASS sample source seals, launch policy, internal links and drift/escape refusals")


if __name__ == "__main__":
    main()
