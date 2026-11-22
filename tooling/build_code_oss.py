#!/usr/bin/env python3
"""Compile the pinned, explicitly partial Code-OSS sample for native Firefly packaging.

Node/pnpm/esbuild are build tools only. The staged application runs on Firefly/Mite;
this command neither packages a Node runtime nor establishes desktop acceptance.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PIN = ROOT / "tooling/code_oss/builtin-extensions-pin.json"
INVENTORY = "ELYTRA-SAMPLE-FILES.json"
ADAPTERS = ("policy-watcher", "pty", "native-keymap", "sqlite", "deviceid", "undici", "fs-copyfile")
ENTRY = """/** Owned launcher for the explicitly partial local Code-OSS sample. */
process.env.VSCODE_DEV = '1';
process.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP = '1';
await import('./src/main.js');
"""


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def run(*args):
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)


def inventory(root, *, physical_only=False):
    """Seal physical files and, where supported, internal dependency/renderer aliases."""
    records = []
    for item in sorted(root.rglob("*")):
        name = item.relative_to(root).as_posix()
        if item.name == ".DS_Store" or name in (INVENTORY, "BUILD.json"):
            continue
        attributes = getattr(item.lstat(), "st_file_attributes", 0)
        is_reparse = bool(attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)
        if physical_only and (is_reparse or item.is_symlink()):
            raise ValueError("Code-OSS physical layout contains a link: " + name)
        if is_reparse and not item.is_symlink():
            raise ValueError("Unsupported Code-OSS reparse point: " + name)
        if item.is_symlink():
            target = os.readlink(item)
            if os.path.isabs(target) or not item.resolve(strict=True).is_relative_to(root):
                raise ValueError("Code-OSS sample symlink escapes stage: " + name)
            records.append({"path": name, "symlink": target})
        elif item.is_file():
            records.append(
                {
                    "path": name,
                    "bytes": item.stat().st_size,
                    "sha256": digest(item),
                    "executable": bool(item.stat().st_mode & 0o111),
                }
            )
        elif not item.is_dir():
            raise ValueError("Unsupported Code-OSS sample entry: " + name)
    return records


def verify_sample(application, *, require_physical=False):
    application = application.resolve(strict=True)
    # These are provenance/entry authorities, not dependency aliases. Reject all
    # metadata links before reading any of them, even links back into this stage;
    # otherwise excluded inventory records could depend on external mutable files.
    for name in ("BUILD.json", INVENTORY, "ELYTRA-COMPILE.json", "package.json", "main.mjs"):
        path = application / name
        if path.is_symlink() or not path.is_file():
            raise ValueError("Code-OSS sample requires physical metadata: " + name)
    metadata = json.loads((application / "BUILD.json").read_text())
    pin = json.loads(PIN.read_text())
    if (
        metadata.get("schema") != 1
        or metadata.get("kind") != "firefly-code-oss-sample"
        or metadata.get("upstream")
        != {key: pin[key] for key in ("repository", "revision", "archiveSha256")}
        or metadata.get("runtime") != "firefly"
        or metadata.get("electronCompat") is not True
        or metadata.get("desktopAcceptance") != "not-established-by-build"
        or metadata.get("builtinExtensions") != "partial-static-only"
        or metadata.get("staticExtensionCount") != pin["staticCount"]
        or metadata.get("unbuiltExtensionCount") != pin["needsBuildCount"]
        or metadata.get("compileSha256") != digest(application / "ELYTRA-COMPILE.json")
        or metadata.get("inventorySha256") != digest(application / INVENTORY)
        or (application / "main.mjs").is_symlink()
        or (application / "main.mjs").read_text() != ENTRY
    ):
        raise ValueError("Code-OSS sample identity or launch policy differs")
    compiled = json.loads((application / "ELYTRA-COMPILE.json").read_text())
    if (
        compiled.get("schema") != 1
        or compiled.get("kind") != "diagnostic-source-tree"
        or compiled.get("adaptations", {}).get("revision") != pin["revision"]
        or not compiled.get("rendererAssets")
        or not compiled.get("nls")
        or len(compiled.get("packageAdaptations", [])) != len(ADAPTERS)
        or not (application / "src/main.js").is_file()
    ):
        raise ValueError("Code-OSS compiler inputs or required build modes differ")
    physical = compiled.get("physicalRendererOutput") is True
    if require_physical and not physical:
        raise ValueError("Windows packaging requires a physical Code-OSS renderer output")
    files = inventory(application, physical_only=physical)
    if json.loads((application / INVENTORY).read_text()) != files:
        raise ValueError("Code-OSS sample payload drifted after compilation")
    if physical:

        def tree(prefix):
            return {
                file["path"].removeprefix(prefix): (file["bytes"], file["sha256"])
                for file in files
                if file["path"].startswith(prefix)
            }

        if tree("out/") != tree("src/"):
            raise ValueError("Code-OSS physical renderer output differs from its source tree")
    return metadata


def prepare_source(source, pin):
    fresh = not source.exists()
    if fresh:
        source.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "--no-checkout", "--depth", "1", pin["repository"], source)
    # git -C can walk upward from an extracted/non-repository directory. Never
    # query or mutate that enclosing checkout on behalf of the requested source.
    repository = subprocess.check_output(
        ["git", "-C", source, "rev-parse", "--show-toplevel"], text=True
    ).strip()
    if Path(repository).resolve() != source.resolve():
        raise ValueError("Code-OSS source is not the root of its own Git checkout")
    remote = subprocess.check_output(
        ["git", "-C", source, "remote", "get-url", "origin"], text=True
    ).strip()
    if remote != pin["repository"]:
        raise ValueError("Code-OSS source remote differs from pin")
    changes = subprocess.check_output(
        [
            "git",
            "-C",
            source,
            "status",
            "--porcelain",
            "--untracked-files=normal",
            "--",
            ".",
            ":(exclude,glob)**/.DS_Store",
        ],
        text=True,
    )
    if changes and not fresh:
        raise ValueError("Code-OSS source has local changes")
    run("git", "-C", source, "fetch", "--depth", "1", "origin", pin["revision"])
    run("git", "-C", source, "-c", "core.autocrlf=false", "checkout", "--detach", pin["revision"])


def archive_file(path, pin):
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_name(path.name + ".download")
        # Exclusive creation avoids adopting or overwriting another stage's download.
        with temporary.open("xb") as destination:
            with urllib.request.urlopen(pin["archiveUrl"], timeout=60) as source:
                remaining = pin["archiveBytes"]
                while remaining:
                    chunk = source.read(min(1024 * 1024, remaining))
                    if not chunk:
                        raise ValueError("Truncated pinned Code-OSS archive")
                    destination.write(chunk)
                    remaining -= len(chunk)
                if source.read(1):
                    raise ValueError("Code-OSS archive exceeds its pin")
        if digest(temporary) != pin["archiveSha256"]:
            raise ValueError("Code-OSS archive hash differs")
        temporary.rename(path)
    if (
        path.is_symlink()
        or path.stat().st_size != pin["archiveBytes"]
        or digest(path) != pin["archiveSha256"]
    ):
        raise ValueError("Code-OSS archive differs from pin")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "build/code-oss-source")
    parser.add_argument("--output", type=Path, default=ROOT / "build/code-oss-app")
    parser.add_argument("--dependencies", type=Path, default=ROOT / "build/code-oss-dependencies")
    parser.add_argument("--overlay", type=Path, default=ROOT / "build/code-oss-release-overlay")
    parser.add_argument("--archive", type=Path, default=ROOT / "build/code-oss-source.tar.gz")
    args = parser.parse_args()
    paths = [
        getattr(args, key).resolve() for key in ("source", "output", "dependencies", "overlay")
    ]
    for index, path in enumerate(paths):
        for other in paths[index + 1 :]:
            if path == other or path in other.parents or other in path.parents:
                parser.error("Source, output, dependencies and overlay must be separate trees")
    source, output, dependencies, overlay = paths
    for path in (output, overlay):
        if path.exists():
            parser.error("Refusing existing output: " + str(path))
        path.parent.mkdir(parents=True, exist_ok=True)
    if not shutil.which("node"):
        parser.error("Pinned Node build tooling is required")
    pin = json.loads(PIN.read_text())
    prepare_source(source, pin)
    if not dependencies.exists():
        dependencies.parent.mkdir(parents=True, exist_ok=True)
        run(
            "node",
            "tooling/code_oss/dependencies.mjs",
            "--source",
            source,
            "--output",
            dependencies,
        )
    run(
        sys.executable,
        "tooling/code_oss/overlay.py",
        "--source",
        source,
        "--output",
        overlay,
        "--libjs-cache",
        "--libjs-engine",
        "--local-desktop",
    )
    run(
        "node",
        "tooling/code_oss/compile.mjs",
        "--source",
        source,
        "--output",
        output,
        "--dependencies",
        dependencies,
        "--overlay",
        overlay,
        "--nls",
        "--renderer-assets",
        *(["--physical-renderer-output"] if os.name == "nt" else []),
        *("--mite-" + name for name in ADAPTERS),
    )
    archive = args.archive.resolve()
    archive_file(archive, pin)
    run(
        sys.executable,
        "tooling/code_oss/builtin-extensions.py",
        "--archive",
        archive,
        "--static-source",
        source,
        "--stage",
        output,
        "--action",
        "stage-static",
    )
    (output / "main.mjs").write_text(ENTRY)
    licenses = output / "licenses"
    licenses.mkdir()
    for name in ("LICENSE.txt", "ThirdPartyNotices.txt", "ELYTRA-LICENSE.txt"):
        shutil.copy2(output / name, licenses / name)
    (output / INVENTORY).write_text(
        json.dumps(inventory(output, physical_only=os.name == "nt"), indent=2) + "\n"
    )
    metadata = {
        "schema": 1,
        "kind": "firefly-code-oss-sample",
        "runtime": "firefly",
        "electronCompat": True,
        "upstream": {key: pin[key] for key in ("repository", "revision", "archiveSha256")},
        "compileSha256": digest(output / "ELYTRA-COMPILE.json"),
        "inventorySha256": digest(output / INVENTORY),
        "builtinExtensions": "partial-static-only",
        "staticExtensionCount": pin["staticCount"],
        "unbuiltExtensionCount": pin["needsBuildCount"],
        "desktopAcceptance": "not-established-by-build",
        "limitations": [
            "webviews",
            "remote",
            "display-capture",
            "engine-code-cache",
            "unbuilt-runtime-extensions",
            "native-addon-ABI",
        ],
    }
    (output / "BUILD.json").write_text(json.dumps(metadata, indent=2) + "\n")
    verify_sample(output)
    print("Compiled partial native Firefly Code-OSS sample:", output)


if __name__ == "__main__":
    main()
