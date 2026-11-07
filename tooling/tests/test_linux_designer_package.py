#!/usr/bin/env python3
"""Require the real Designer edit/Monaco/control workflow from an extracted Linux archive."""

import _support

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from native_package import elf
from _linux_package import packaged_app
from _x11 import isolated_display, managed_window_manager
from test_renderer import ROOT, environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--mite-build", type=Path, required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument(
        "--artifacts-dir", type=Path, default=ROOT / "build/linux-designer-package-artifacts"
    )
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/linux-designer-packages")
    args = parser.parse_args()
    elf.native_tools()
    isolated_display(os.environ)
    output = args.output_dir.resolve()
    if output.exists():
        parser.error("Choose a fresh output directory for accepted candidate archives")
    artifacts = args.artifacts_dir.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    build = args.build.resolve(strict=True)
    mite = args.mite_build.resolve(strict=True)
    application = args.app.resolve(strict=True)
    metadata = json.loads((application / "BUILD.json").read_text())
    assert metadata["webAssets"] == "bundled", "Build the actual bundled Designer first"
    assert metadata["dependencies"]["mode"] == "production", metadata
    with tempfile.TemporaryDirectory(prefix="firefly-linux-designer-") as directory:
        temporary = Path(directory)
        root, archive = packaged_app(
            build,
            mite,
            application,
            temporary,
            artifacts,
            "web-component-designer",
            version=args.version,
        )
        runtime = temporary / "runtime"
        runtime.mkdir(mode=0o700)
        env = {
            **environment(directory),
            "XDG_RUNTIME_DIR": str(runtime),
            "QT_SCALE_FACTOR": "1",
            "QT_SCREEN_SCALE_FACTORS": "1",
        }
        # Use the same server/security/edit/Monaco/window assertions as macOS.
        # There is no reduced Linux-only definition of Designer acceptance.
        with managed_window_manager(env, artifacts / "window-manager.log"):
            result = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tooling/tests/test_designer.py"),
                    "--mite",
                    str(mite / "bin/mite"),
                    "--app",
                    str(root / "app"),
                    "--launcher",
                    str(root / "launch"),
                ],
                env=env,
                capture_output=True,
                text=True,
                timeout=150,
            )
            (artifacts / "acceptance.log").write_text(result.stdout + "\nSTDERR:\n" + result.stderr)
            print(result.stdout)
            assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
        # Publish an Actions candidate only after archive and native app gates pass.
        # Preserve original BUILD/source/version data; do not turn it into a release.
        output.mkdir(parents=True)
        target = output / f"web-component-designer-{args.version}-linux-x64.tar.gz"
        shutil.copy2(archive, target)
        assert elf.digest(target) == elf.digest(archive), "Candidate changed during copy"
        Path(str(target) + ".sha256").write_text(elf.digest(target) + "  " + target.name + "\n")
        shutil.copy2(root / "BUILD.json", output / "BUILD.json")
    print(
        "PASS extracted Linux Designer archive, real edit/Monaco/HTML controls and candidate copy"
    )


if __name__ == "__main__":
    main()
