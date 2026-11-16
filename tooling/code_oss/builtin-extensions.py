#!/usr/bin/env python3
"""Plan or explicitly build/stage real pinned upstream built-in extensions.

The default is read-only. One owned workspace holds verified original sources and
build dependencies; the accepted root production dependency tree is never used as
an installation target. No extension manifest or runtime entry is fabricated.
"""

import argparse
import hashlib
import json
import os
import platform
import sys
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
PIN = json.loads(Path(__file__).with_name("builtin-extensions-pin.json").read_text())
OWNER = "ELYTRA-BUILTIN-WORKSPACE.json"
INPUTS = "ELYTRA-BUILTIN-INPUTS.json"
BUILT = "ELYTRA-BUILTIN-BUILD.json"
STAGED = "ELYTRA-BUILTIN-EXTENSIONS.json"
MAX_FILES, MAX_BYTES = 100000, 1024 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def relative(value):
    path = PurePosixPath(value)
    require(
        value and "\0" not in value and "\\" not in value and not path.is_absolute(),
        "Invalid relative path",
    )
    require(all(part not in ("", ".", "..") for part in value.split("/")), "Unsafe relative path")
    return path


def physical(path, missing=False):
    path = Path(os.path.abspath(path))
    for part in (path, *path.parents):
        require(not part.is_symlink(), f"Symlink in owned path: {part}")
    if not missing:
        require(path.exists(), f"Missing path: {path}")
    return path


def dump(path, value):
    payload = json.dumps(value, indent=2) + "\n"
    temporary = path.with_name(path.name + ".writing")
    require(not temporary.exists() and not temporary.is_symlink(), "Pending metadata write exists")
    with temporary.open("x", encoding="utf-8") as stream:
        stream.write(payload)
    temporary.replace(path)


def archive_inventory(archive):
    archive = physical(archive)
    require(
        archive.is_file() and archive.stat().st_size == PIN["archiveBytes"], "Wrong archive size"
    )
    require(digest(archive) == PIN["archiveSha256"], "Code-OSS archive differs from immutable pin")
    prefix = "vscode-" + PIN["revision"]
    records, manifests, captured, seen = [], {}, {}, set()
    with tarfile.open(archive, "r:gz") as source:
        require(source.pax_headers.get("comment") == PIN["revision"], "Wrong archive revision")
        for item in source:
            parts = PurePosixPath(item.name).parts
            require(parts and parts[0] == prefix, "Unexpected archive root")
            if len(parts) == 1:
                require(item.isdir(), "Archive root must be directory")
                continue
            name = "/".join(parts[1:])
            relative(name)
            require(name not in seen, "Duplicate archive entry")
            seen.add(name)
            if item.isdir():
                continue
            if item.issym():
                target = relative(item.linkname)
                require(len(target.parts) == 1, "Unexpected upstream symbolic link")
                records.append({"path": name, "symlink": item.linkname})
                continue
            require(item.isfile() and item.size <= 64 * 1024 * 1024, "Unsupported archive member")
            data = source.extractfile(item).read()
            require(len(data) == item.size, "Truncated archive file")
            records.append(
                {
                    "path": name,
                    "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "executable": bool(item.mode & 0o111),
                }
            )
            if name in ("product.json", "package.json", ".nvmrc"):
                captured[name] = data
            if len(parts) == 4 and parts[1] == "extensions" and parts[3] == "package.json":
                manifests[parts[2]] = json.loads(data)
    regular = [entry for entry in records if "symlink" not in entry]
    require(
        len(regular) == PIN["sourceFiles"]
        and sum(entry["bytes"] for entry in regular) == PIN["sourceBytes"],
        "Archive inventory differs",
    )
    require(len(records) < MAX_FILES, "Source file limit")
    require(captured[".nvmrc"].decode().strip() == PIN["nodeVersion"], "Node build pin differs")
    require(
        json.loads(captured["package.json"])["packageManager"] == "pnpm@" + PIN["pnpmVersion"],
        "Package-manager pin differs",
    )
    own_pin = json.loads((ROOT / "vendor/code-oss-tests/manifest.json").read_text())
    require(own_pin["revision"] == PIN["revision"], "Code-OSS source pins disagree")
    entries = []
    for name, manifest in sorted(manifests.items()):
        renderer = manifest.get("contributes", {}).get("notebookRenderer", [])
        renderer_entries = [
            entry["entrypoint"] for entry in renderer if isinstance(entry.get("entrypoint"), str)
        ]
        needs_build = bool(manifest.get("main") or manifest.get("browser") or renderer_entries)
        entries.append(
            {
                "directory": name,
                "id": manifest.get("publisher", "") + "." + manifest["name"],
                "version": manifest["version"],
                "classification": "needs-build" if needs_build else "static-contributions",
                "main": manifest.get("main"),
                "browser": manifest.get("browser"),
                "rendererEntries": renderer_entries,
                "upstreamExcluded": name in PIN["upstreamExcluded"],
                "nativeRuntime": name in PIN["nativeExtensions"],
            }
        )
    require(
        sum(entry["classification"] == "static-contributions" for entry in entries)
        == PIN["staticCount"],
        "Static extension classification drift",
    )
    require(
        sum(entry["classification"] == "needs-build" for entry in entries)
        == PIN["needsBuildCount"],
        "Runtime extension classification drift",
    )
    product = json.loads(captured["product.json"])
    marketplace = product.get("builtInExtensions", [])
    require(
        all(
            len(item.get("sha256", "")) == 64
            and item.get("version")
            and not item.get("vsix")
            and not item.get("platformSpecific")
            for item in marketplace
        ),
        "Unreviewed product download declaration",
    )
    return {"records": records, "extensions": entries, "marketplace": marketplace}


def commands(args):
    workspace = args.workspace
    common = [args.node, str(args.pnpm)]
    installs = []
    for folder in ("", "build", "extensions") + (
        ("extensions/copilot",) if args.include_copilot else ()
    ):
        command = common + [
            "--dir",
            str(workspace / folder),
            "install",
            "--frozen-lockfile",
            "--ignore-scripts",
            "--prod=false",
            "--store-dir",
            str(args.store),
        ]
        if folder in ("", "build"):
            command += ["--filter", "."]
        installs.append(command)
    gulp = [
        args.node,
        "--experimental-strip-types",
        "--max-old-space-size=8192",
        str(workspace / "node_modules/gulp/bin/gulp.js"),
        "--cwd",
        str(workspace),
        "--gulpfile",
        str(workspace / "build/gulpfile.extensions.ts"),
    ]
    tasks = [gulp + ["compile-extension-media"], gulp + ["compile-extensions-build"]]
    if args.include_copilot:
        tasks.append(gulp + ["compile-copilot-extension-build"])
    return installs + tasks


def verify_workspace(workspace, inventory):
    workspace = physical(workspace)
    marker = json.loads(physical(workspace / OWNER).read_text())
    require(
        marker.get("kind") == "owned-code-oss-extension-workspace"
        and marker.get("archiveSha256") == PIN["archiveSha256"]
        and marker.get("revision") == PIN["revision"],
        "Refuse unowned workspace",
    )
    require(marker.get("state") != "preparing", "Workspace preparation incomplete")
    stored = json.loads(physical(workspace / INPUTS).read_text())
    require(stored == inventory["records"], "Workspace source inventory differs")
    for record in stored:
        target = workspace / record["path"]
        if "symlink" in record:
            # One original fixture symlink remains internal and is never staged.
            require(
                target.is_symlink() and os.readlink(target) == record["symlink"],
                "Source symlink drift",
            )
        else:
            physical(target)
            require(
                target.is_file() and digest(target) == record["sha256"],
                f"Original source changed: {record['path']}",
            )
    return marker


def prepare(args, inventory):
    workspace = physical(args.workspace, missing=True)
    if workspace.exists():
        verify_workspace(workspace, inventory)
        return
    workspace.mkdir(parents=True, mode=0o700)
    dump(
        workspace / OWNER,
        {
            "schema": 1,
            "kind": "owned-code-oss-extension-workspace",
            "revision": PIN["revision"],
            "archiveSha256": PIN["archiveSha256"],
            "state": "preparing",
        },
    )
    with tarfile.open(args.archive, "r:gz") as source:
        for item in source:
            parts = PurePosixPath(item.name).parts
            if len(parts) == 1:
                continue
            target = workspace.joinpath(*parts[1:])
            if item.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif item.issym():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.symlink_to(item.linkname)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("xb") as output, source.extractfile(item) as input_file:
                    shutil.copyfileobj(input_file, output, 1024 * 1024)
                target.chmod(0o755 if item.mode & 0o111 else 0o644)
    dump(workspace / INPUTS, inventory["records"])
    dump(
        workspace / OWNER,
        {
            "schema": 1,
            "kind": "owned-code-oss-extension-workspace",
            "revision": PIN["revision"],
            "archiveSha256": PIN["archiveSha256"],
            "state": "prepared",
        },
    )
    verify_workspace(workspace, inventory)


def files_under(root):
    root = physical(root)
    result, total = [], 0
    for folder, directories, files in os.walk(root, followlinks=False):
        for name in directories:
            require(not (Path(folder) / name).is_symlink(), "Generated directory symlink")
        for name in sorted(files):
            target = physical(Path(folder) / name)
            require(target.is_file(), "Generated non-file")
            if name == ".DS_Store":
                continue
            total += target.stat().st_size
            require(
                len(result) < MAX_FILES and total <= MAX_BYTES,
                "Generated extension budget exceeded",
            )
            result.append(
                {
                    "path": target.relative_to(root).as_posix(),
                    "bytes": target.stat().st_size,
                    "sha256": digest(target),
                    "executable": bool(target.stat().st_mode & 0o111),
                }
            )
    return sorted(result, key=lambda entry: entry["path"])


def entry_exists(directory, value):
    require(isinstance(value, str) and value.startswith("./"), "Unreviewed extension entry path")
    name = value[2:]
    relative(name)
    return any((directory / (name + suffix)).is_file() for suffix in ("", ".js", ".cjs", ".mjs"))


def verify_built(args, inventory, require_selection=True):
    marker = json.loads(physical(args.workspace / BUILT).read_text())
    require(
        marker.get("state") == "built-not-runtime-accepted"
        and marker.get("archiveSha256") == PIN["archiveSha256"]
        and marker.get("nodeVersion") == PIN["nodeVersion"]
        and marker.get("pnpmVersion") == PIN["pnpmVersion"]
        and marker.get("revision") == PIN["revision"]
        and marker.get("platform") == sys.platform
        and marker.get("architecture") == platform.machine()
        and isinstance(marker.get("includeCopilot"), bool)
        and (not require_selection or marker["includeCopilot"] == args.include_copilot),
        "Missing matching successful upstream build",
    )
    output = args.workspace / ".build/extensions"
    require(files_under(output) == marker["outputs"], "Built extension output drift")
    expected = {
        entry["directory"]
        for entry in inventory["extensions"]
        if not entry["upstreamExcluded"]
        or (marker["includeCopilot"] and entry["directory"] == "copilot")
    }
    expected |= {item["name"] for item in inventory["marketplace"]}
    actual = {
        item.name for item in output.iterdir() if item.is_dir() and item.name != "node_modules"
    }
    require(actual == expected, "Upstream packaged extension set differs")
    for name in sorted(expected):
        directory = output / name
        manifest = json.loads(physical(directory / "package.json").read_text())
        if manifest.get("main"):
            require(
                entry_exists(directory, manifest["main"]), f"Missing actual desktop entry: {name}"
            )
        for renderer in manifest.get("contributes", {}).get("notebookRenderer", []):
            require(
                entry_exists(directory, renderer["entrypoint"]),
                f"Missing actual notebook renderer: {name}",
            )
    return marker


def build(args, inventory):
    verify_workspace(args.workspace, inventory)
    actual_node = subprocess.check_output([args.node, "--version"], text=True).strip()
    require(actual_node == "v" + PIN["nodeVersion"], "Use the pinned build-only Node version")
    pnpm = physical(args.pnpm)
    pnpm_package = json.loads((pnpm.parent.parent / "package.json").read_text())
    require(
        pnpm_package["name"] == "pnpm" and pnpm_package["version"] == PIN["pnpmVersion"],
        "Wrong pnpm build tool",
    )
    # Reused marketplace caches must be owned by the previous successful output
    # inventory; upstream's own cache test checks version only, not file bytes.
    cache = args.workspace / ".build/builtInExtensions"
    require(
        not cache.exists(), "Unverified marketplace cache exists; do not adopt version-only cache"
    )
    # Upstream clean tasks own generated files, not Finder metadata. Refuse
    # before invoking any cleanup if the user has visited this workspace.
    require(
        not any(args.workspace.rglob(".DS_Store")),
        "Finder metadata present; refusing upstream cleanup",
    )
    previous = args.workspace / BUILT
    if previous.exists():
        verify_built(args, inventory, require_selection=False)
        previous.unlink()
    require(
        not os.environ.get("NODE_OPTIONS"), "NODE_OPTIONS would change the pinned build execution"
    )
    environment = dict(os.environ)
    environment["BUILD_SOURCEVERSION"] = PIN["revision"]
    environment["VSCODE_QUALITY"] = "stable"  # never use upstream's checksum-free latest path
    environment.pop("VSCODE_ARCH", None)
    # No implicit lifecycle install: commands and downloaded archives remain
    # explicit, frozen inputs. Missing native build tools fail normally.
    for command in commands(args):
        subprocess.run(command, cwd=args.workspace, env=environment, check=True)
    verify_workspace(args.workspace, inventory)
    output = args.workspace / ".build/extensions"
    report = {
        "schema": 1,
        "state": "built-not-runtime-accepted",
        "platform": sys.platform,
        "architecture": platform.machine(),
        "revision": PIN["revision"],
        "archiveSha256": PIN["archiveSha256"],
        "nodeVersion": PIN["nodeVersion"],
        "pnpmVersion": PIN["pnpmVersion"],
        "includeCopilot": args.include_copilot,
        "commands": commands(args),
        "marketplace": inventory["marketplace"],
        "outputs": files_under(output),
    }
    dump(previous, report)
    verify_built(args, inventory)


def stage(args, inventory, static_only):
    require(args.stage is not None, "--stage is required for staging")
    destination = physical(args.stage)
    require(
        destination != args.workspace
        and destination not in args.workspace.parents
        and args.workspace not in destination.parents,
        "Stage overlaps build workspace",
    )
    compile_manifest = json.loads(physical(destination / "ELYTRA-COMPILE.json").read_text())
    require(
        compile_manifest.get("kind") == "diagnostic-source-tree"
        and compile_manifest.get("adaptations", {}).get("revision") == PIN["revision"],
        "Stage has no matching owned Code-OSS compile manifest",
    )
    verify_workspace(args.workspace, inventory)
    if not static_only:
        verify_built(args, inventory)
    planned = {}
    if static_only:
        for extension in inventory["extensions"]:
            if extension["classification"] != "static-contributions":
                continue
            folder = args.workspace / "extensions" / extension["directory"]
            for record in files_under(folder):
                planned["extensions/" + extension["directory"] + "/" + record["path"]] = (
                    folder / record["path"],
                    record,
                )
    else:
        built = args.workspace / ".build/extensions"
        market_names = {item["name"] for item in inventory["marketplace"]}
        for record in files_under(built):
            # The actual dev scanner reads product VSIXs from .build, while
            # source-built extensions and shared dependencies live in extensions.
            name = record["path"].split("/")[0]
            target = (
                ".build/builtInExtensions/" if name in market_names else "extensions/"
            ) + record["path"]
            planned[target] = (built / record["path"], record)
    old = None
    old_paths = set()
    owned_manifest = destination / STAGED
    if owned_manifest.exists():
        old = json.loads(physical(owned_manifest).read_text())
        require(
            old.get("kind") == "owned-built-in-extension-stage"
            and old.get("revision") == PIN["revision"]
            and old.get("archiveSha256") == PIN["archiveSha256"],
            "Unknown prior extension inventory",
        )
        for record in old["files"]:
            relative(record["path"])
            require(
                record["path"].startswith(("extensions/", ".build/builtInExtensions/")),
                "Prior ownership escaped extension roots",
            )
            require(record["path"] not in old_paths, "Duplicate prior ownership")
            target = physical(destination / record["path"])
            require(
                target.is_file() and digest(target) == record["sha256"], "Edited staged extension"
            )
            old_paths.add(record["path"])
    for root in (destination / "extensions", destination / ".build/builtInExtensions"):
        if root.exists():
            for record in files_under(root):
                require(
                    (root / record["path"]).relative_to(destination).as_posix() in old_paths,
                    "Unowned existing extension file",
                )
    for name in planned:
        physical(destination / name, missing=True)
    staged_extensions = []
    for name, (input_file, record) in sorted(planned.items()):
        parts = name.split("/")
        top_manifest = (
            len(parts) == 3 and parts[0] == "extensions" and parts[1] != "node_modules"
        ) or (len(parts) == 4 and parts[:2] == [".build", "builtInExtensions"])
        if not top_manifest or parts[-1] != "package.json":
            continue
        manifest = json.loads(input_file.read_text())
        item = {
            "id": manifest.get("publisher", "") + "." + manifest["name"],
            "version": manifest["version"],
            "location": "/".join(parts[:-1]),
            "manifestSha256": record["sha256"],
            "activation": "unverified",
            "main": manifest.get("main"),
            "desktopEntryPresent": entry_exists(input_file.parent, manifest["main"])
            if manifest.get("main")
            else None,
            "browser": manifest.get("browser"),
            "browserEntryPresent": entry_exists(input_file.parent, manifest["browser"])
            if manifest.get("browser")
            else None,
            "nativeBinaryFiles": [
                file
                for file in planned
                if file.startswith("/".join(parts[:-1]) + "/") and file.endswith(".node")
            ],
        }
        staged_extensions.append(item)
    report = {
        "schema": 1,
        "kind": "owned-built-in-extension-stage",
        "revision": PIN["revision"],
        "archiveSha256": PIN["archiveSha256"],
        "mode": "partial-static-only" if static_only else "upstream-standard-build",
        "runtimeAcceptance": "not-established",
        "includeCopilot": args.include_copilot,
        "stagedExtensions": staged_extensions,
        "notStaged": [
            entry
            for entry in inventory["extensions"]
            if not any(item["id"] == entry["id"] for item in staged_extensions)
        ],
        "extensions": inventory["extensions"],
        "marketplace": inventory["marketplace"],
        "marketplaceStatus": [
            {
                "name": item["name"],
                "staged": not static_only,
                "archiveVerification": "upstream-pinned-checksum"
                if not static_only
                else "not-downloaded",
            }
            for item in inventory["marketplace"]
        ],
        "files": [{**record, "path": name} for name, (_, record) in sorted(planned.items())],
    }
    # Incomplete copy must not retain a completed ownership claim.
    if owned_manifest.exists():
        backup = destination / (STAGED + ".previous")
        require(not backup.is_symlink(), "Extension backup is symlink")
        owned_manifest.replace(backup)
    for name in old_paths - planned.keys():
        (destination / name).unlink()
    for name, (source, record) in sorted(planned.items()):
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or digest(target) != record["sha256"]:
            shutil.copyfile(source, target)
        target.chmod(0o755 if record["executable"] else 0o644)
    dump(owned_manifest, sealed(report))


def sealed(report):
    payload = json.dumps(report, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return {
        **report,
        "seal": {"algorithm": "sha256", "digest": hashlib.sha256(payload.encode()).hexdigest()},
    }


def verify_stage(args):
    """Compose the compiler's existing checks without adopting its owned outputs."""
    script = """
import { verifiedRefresh } from './tooling/code_oss/compile-refresh.mjs';
import { verifyPackageAdditions } from './tooling/code_oss/package-adaptations.mjs';
const stage = process.argv[1];
const core = verifiedRefresh(stage);
verifyPackageAdditions(stage, core.previous.packageAdaptations ?? [],
  (core.previous.packageAdaptations ?? []).map(record => ({plan: {record}})));
console.log(JSON.stringify({ core: core.manifestSha256, resources: core.builtinExtensions }));
"""
    result = subprocess.run(
        [args.node, "--input-type=module", "-e", script, str(args.stage)],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)


def static_source(args, inventory):
    source = physical(args.static_source)
    require(source.is_dir(), "Static source must be an existing physical directory")
    selected = {"extensions/" + item["directory"] for item in PIN["staticExtensions"]}
    originals = [
        item
        for item in inventory["records"]
        if item["path"].rsplit("/", 1)[0] in selected
        or any(item["path"].startswith(folder + "/") for folder in selected)
    ]
    require(
        sorted(originals, key=lambda item: item["path"]) == PIN["staticFiles"],
        "Maintained static source index differs from archive",
    )
    directories = {
        str(parent)
        for item in PIN["staticFiles"]
        for parent in PurePosixPath(item["path"]).parents
        if str(parent) != "."
    }
    actual = []
    for folder in sorted(selected):
        for parent, children, _ in os.walk(physical(source / folder), followlinks=False):
            for child in children:
                candidate = physical(Path(parent) / child)
                require(
                    candidate.relative_to(source).as_posix() in directories,
                    "Unknown static source directory",
                )
        for record in files_under(physical(source / folder)):
            actual.append({**record, "path": folder + "/" + record["path"]})
    require(
        sorted(actual, key=lambda item: item["path"]) == PIN["staticFiles"],
        "Extracted static source differs from immutable archive",
    )
    require(
        args.action in ("plan", "stage-static"), "--static-source only supports plan/stage-static"
    )
    require(not args.include_copilot, "Static source does not include Copilot")
    if args.stage is None:
        require(args.action == "plan", "--stage is required for staging")
        return
    args.stage = physical(args.stage)
    require(
        args.stage != source
        and args.stage not in source.parents
        and source not in args.stage.parents,
        "Stage and static source overlap",
    )
    before = verify_stage(args)
    core = json.loads((args.stage / "ELYTRA-COMPILE.json").read_text())
    require(
        core.get("adaptations", {}).get("revision") == PIN["revision"], "Wrong compiled source pin"
    )
    if args.action == "plan":
        return
    if before["resources"] is not None:
        require(
            before["resources"]["mode"] == "partial-static-only",
            "Refuse to replace a built extension stage",
        )
        return  # Exact pinned inventory was already reverified; no writes.
    pending = args.stage / ".elytra-builtin-static.pending"
    receipt = args.stage / (STAGED + ".pending")
    lock = args.stage / ".ELYTRA-BUILTIN-EXTENSIONS.lock"
    physical(lock, missing=True)
    lock.mkdir()  # Never steal or remove an earlier transaction's lock.
    try:
        physical(pending, missing=True)
        physical(receipt, missing=True)
        require(
            not pending.exists() and not receipt.exists(), "Incomplete extension transaction exists"
        )
        pending.mkdir(mode=0o700)
        for record in PIN["staticFiles"]:
            target = pending / record["path"].removeprefix("extensions/")
            target.parent.mkdir(parents=True, exist_ok=True)
            with (
                physical(source / record["path"]).open("rb") as original,
                target.open("xb") as output,
            ):
                shutil.copyfileobj(original, output)
            target.chmod(0o755 if record["executable"] else 0o644)
        copied = [
            {**record, "path": "extensions/" + record["path"]} for record in files_under(pending)
        ]
        require(copied == PIN["staticFiles"], "Copied static resources differ")
        staged = []
        for item in PIN["staticExtensions"]:
            location = "extensions/" + item["directory"]
            manifest_path = pending / item["directory"] / "package.json"
            manifest = json.loads(manifest_path.read_text())
            staged.append(
                {
                    "id": item["id"],
                    "version": item["version"],
                    "location": location,
                    "manifestSha256": digest(manifest_path),
                    "activation": "unverified",
                    "main": manifest.get("main"),
                    "browser": manifest.get("browser"),
                    "desktopEntryPresent": None,
                    "browserEntryPresent": None,
                    "nativeBinaryFiles": [],
                }
            )
        report = sealed(
            {
                "schema": 1,
                "kind": "owned-built-in-extension-stage",
                "revision": PIN["revision"],
                "archiveSha256": PIN["archiveSha256"],
                "mode": "partial-static-only",
                "runtimeAcceptance": "not-established",
                "includeCopilot": False,
                "stagedExtensions": staged,
                "notStaged": PIN["needsBuildExtensions"],
                "extensions": inventory["extensions"],
                "marketplace": inventory["marketplace"],
                "marketplaceStatus": [
                    {"name": item["name"], "staged": False, "archiveVerification": "not-downloaded"}
                    for item in inventory["marketplace"]
                ],
                "preparedFromCompileSha256": before["core"],
                "files": PIN["staticFiles"],
            }
        )
        # The copied bytes were verified against the archive; recheck compiler ownership.
        require(
            digest(args.stage / "ELYTRA-COMPILE.json") == before["core"],
            "Compile changed during staging",
        )
        with receipt.open("x", encoding="utf-8") as output:
            output.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
            output.flush()
            os.fsync(output.fileno())
        rename_exclusive(pending, args.stage / "extensions")
        rename_exclusive(receipt, args.stage / STAGED)  # Ownership published LAST.
        require(verify_stage(args)["core"] == before["core"], "Compile changed during publication")
    finally:
        lock.rmdir()  # Only this invocation's empty lock; failed data stays bounded for inspection.


def rename_exclusive(source, destination):
    """Atomic non-replacing rename, including collisions with empty directories."""
    import ctypes

    if sys.platform == "darwin":
        library = ctypes.CDLL(None, use_errno=True)
        function = library.renamex_np
        function.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        result = function(os.fsencode(source), os.fsencode(destination), 4)  # RENAME_EXCL
    elif sys.platform.startswith("linux"):
        library = ctypes.CDLL(None, use_errno=True)
        require(hasattr(library, "renameat2"), "Atomic no-replace rename unavailable")
        function = library.renameat2
        function.argtypes = [
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_uint,
        ]
        result = function(-100, os.fsencode(source), -100, os.fsencode(destination), 1)
    elif sys.platform == "win32":
        os.rename(source, destination)  # Windows rename refuses an existing destination.
        return
    else:
        raise ValueError("Atomic no-replace staging unsupported on this platform")
    if result:
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error), str(destination))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--workspace", type=Path)
    parser.add_argument("--static-source", type=Path)
    parser.add_argument("--store", type=Path)
    parser.add_argument("--stage", type=Path)
    parser.add_argument("--node", default=shutil.which("node") or "node")
    parser.add_argument(
        "--pnpm", type=Path, default=ROOT / "tooling/code_oss/node_modules/pnpm/bin/pnpm.mjs"
    )
    parser.add_argument("--include-copilot", action="store_true")
    parser.add_argument(
        "--action",
        choices=("plan", "prepare", "build", "stage-static", "stage-built"),
        default="plan",
    )
    args = parser.parse_args()
    for name in ("archive", "workspace", "store", "pnpm", "static_source", "stage"):
        if getattr(args, name) is not None:
            setattr(args, name, Path(os.path.abspath(getattr(args, name))))
    inventory = archive_inventory(args.archive)
    if args.static_source is not None:
        require(
            args.workspace is None and args.store is None,
            "Static source cannot adopt a workspace/store",
        )
        static_source(args, inventory)
        print(
            json.dumps(
                {
                    "action": args.action,
                    "writes": args.action != "plan",
                    "staticSource": str(args.static_source),
                    "staticFiles": len(PIN["staticFiles"]),
                    "staticBytes": PIN["staticBytes"],
                    "needsBuild": PIN["needsBuildCount"],
                    "runtimeAcceptance": "not-established",
                    "commands": [],
                },
                indent=2,
            )
        )
        return
    require(
        args.workspace is not None and args.store is not None,
        "--workspace and --store required without --static-source",
    )
    require(
        args.workspace != args.store
        and args.workspace not in args.store.parents
        and args.store not in args.workspace.parents,
        "Workspace and shared store overlap",
    )
    inventory = archive_inventory(args.archive)
    if args.action == "prepare":
        prepare(args, inventory)
    elif args.action != "plan":
        verify_workspace(args.workspace, inventory)
        lock = args.workspace / "ELYTRA-BUILTIN-WORKSPACE.lock"
        require(not lock.is_symlink(), "Workspace lock is symlink")
        # Never steal a stale/concurrent owner's lock or remove its files.
        lock.mkdir()
        try:
            if args.action == "build":
                build(args, inventory)
            else:
                stage(args, inventory, args.action == "stage-static")
        finally:
            lock.rmdir()
    print(
        json.dumps(
            {
                "action": args.action,
                "writes": args.action != "plan",
                "revision": PIN["revision"],
                "archiveSha256": PIN["archiveSha256"],
                "workspace": str(args.workspace),
                "sourceFiles": PIN["sourceFiles"],
                "sourceBytes": PIN["sourceBytes"],
                "staticContributions": PIN["staticCount"],
                "needsBuild": PIN["needsBuildCount"],
                "extensions": inventory["extensions"],
                "productVSIX": inventory["marketplace"],
                "commands": commands(args),
                "runtimeAcceptance": "not-established",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
