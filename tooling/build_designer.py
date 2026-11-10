#!/usr/bin/env python3
"""Build and stage the pinned upstream Designer demo with its Firefly main module."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from npm_payload import copy_dependencies
from designer_shell import apply_index_overlay, shell_metadata

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples/web-component-designer"


def run(*args, cwd=ROOT):
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def source_changes(source):
    # Finder metadata is unrelated to the pinned source. Preserve it in place,
    # including tracked files, while every other tracked/untracked change blocks.
    return subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=normal",
            "--",
            ".",
            ":(exclude,glob)**/.DS_Store",
        ],
        cwd=source,
        text=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "build/designer-source")
    parser.add_argument("--output", type=Path, default=ROOT / "build/designer-app")
    parser.add_argument(
        "--dependencies",
        choices=["production", "all"],
        default="production",
        help="Stage runtime dependencies only (default), or include development packages",
    )
    parser.add_argument(
        "--bundle",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Bundle/minify browser modules (default); --no-bundle retains the module tree",
    )
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if output.exists():
        parser.error("Output already exists; choose a fresh staging directory")
    if source == output or source in output.parents:
        parser.error("Output must be separate from the source checkout")
    pin = json.loads((EXAMPLE / "upstream.json").read_text())
    fresh = not source.exists()
    if fresh:
        source.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "--no-checkout", "--depth", "1", pin["repository"], source)
    remote = subprocess.check_output(
        ["git", "remote", "get-url", "origin"], cwd=source, text=True
    ).strip()
    if remote != pin["repository"]:
        parser.error("Source remote differs from pinned repository")
    dirty = source_changes(source)
    if dirty and not fresh:
        parser.error("Source checkout contains local changes")
    run("git", "fetch", "--depth", "1", "origin", pin["revision"], cwd=source)
    run("git", "checkout", "--detach", pin["revision"], cwd=source)
    if (
        hashlib.sha256((source / "package-lock.json").read_bytes()).hexdigest()
        != pin["packageLockSha256"]
    ):
        parser.error("Pinned package-lock hash differs")
    npm = shutil.which("npm.cmd" if os.name == "nt" else "npm")
    if not npm:
        parser.error("npm is a build-time prerequisite")
    run(npm, "ci", "--ignore-scripts", "--no-audit", "--no-fund", cwd=source)
    # These two reviewed source scripts are explicit, not implicit lifecycle hooks.
    run(npm, "run", "bundleMonaco", cwd=source)
    run(npm, "run", "build", cwd=source)
    required = [
        "dist/appShell.js",
        "node_modules/monaco-editor/esm/vs/editor/editor.main.min.js",
        "node_modules/monaco-editor/esm/vs/editor/editor.worker.js",
    ]
    for name in required:
        if not (source / name).is_file():
            raise RuntimeError("Build output missing: " + name)
    web = output / "web"
    web.mkdir(parents=True)
    for name in ["index.html", "manifest.json", "favicon.ico", "assets", "dist"]:
        item = source / name
        if item.is_dir():
            shutil.copytree(item, web / name, ignore=shutil.ignore_patterns(".DS_Store", ".bin"))
        else:
            shutil.copy2(item, web / name)
    dependency_info = copy_dependencies(
        source / "node_modules",
        web / "node_modules",
        source / "package-lock.json",
        args.dependencies,
    )
    # Own shell overlay only; the pinned demo checkout and app implementation stay intact.
    index = web / "index.html"
    index.write_text(apply_index_overlay(index.read_text()))
    for name in ["window-controls.mjs", "window-controls.css"]:
        shutil.copy2(EXAMPLE / name, web / name)
    for name in ["main.mjs", "server.mjs", "smoke.mjs", "upstream.json"]:
        shutil.copy2(EXAMPLE / name, output / name)
    notices = output / "licenses"
    notices.mkdir()
    shutil.copy2(source / "LICENSE", notices / "designer-MIT.txt")
    shutil.copy2(source / "package-lock.json", output / "designer-package-lock.json")
    if args.bundle:
        run("node", ROOT / "tooling/bundle_designer.mjs", source, web)
    # Browser bundles retain npm notices outside the public web tree.
    (output / "BUILD.json").write_text(
        json.dumps(
            {
                "upstream": pin,
                "designerShell": shell_metadata(output),
                "runtime": "firefly",
                "webRoot": "web",
                "dependencies": dependency_info,
                "webAssets": "bundled" if args.bundle else "modules",
                "status": "compiled; native UI/features require separate validation",
            },
            indent=2,
        )
        + "\n"
    )
    print("Staged compiled Designer application at", output)


if __name__ == "__main__":
    main()
