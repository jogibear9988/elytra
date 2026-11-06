#!/usr/bin/env python3
"""Copy a locked installed npm payload, optionally omitting dev-only packages.

npm lockfile v2/v3 marks exclusively development packages with dev:true. Shared
production packages and production optional packages (including devOptional) stay.
Package contents/notices are retained; this does not prune runtime files by suffix.
"""

import json
from pathlib import Path, PurePosixPath
import shutil


def copy_dependencies(source, destination, lockfile, mode="production"):
    source = Path(source).resolve()
    destination = Path(destination)
    if mode not in ("production", "all"):
        raise ValueError("Unknown dependency mode")
    lock = json.loads(Path(lockfile).read_text())
    if lock.get("lockfileVersion") not in (2, 3) or not isinstance(lock.get("packages"), dict):
        raise ValueError("A v2/v3 npm packages lock is required")
    if destination.exists():
        raise ValueError("Dependency destination must be fresh")
    if destination.resolve() == source or source in destination.resolve().parents:
        raise ValueError("Payload must be outside installed dependencies")
    destination.mkdir(parents=True)
    copied = []
    omitted = []
    for name, entry in sorted(lock["packages"].items()):
        if not name:
            continue
        parts = PurePosixPath(name).parts
        if (
            not parts
            or parts[0] != "node_modules"
            or any(p in (".", "..") for p in parts)
            or "\\" in name
            or name.startswith("/")
        ):
            raise ValueError("Unsupported locked package path: " + name)
        if entry.get("link"):
            raise ValueError("Linked packages require a separate reviewed payload adapter")
        relative = Path(*parts[1:])
        package = source / relative
        if mode == "production" and entry.get("dev"):
            omitted.append(name)
            continue
        if not package.is_dir():
            if entry.get("optional"):
                continue
            raise ValueError("Required installed package missing: " + name)
        if package.is_symlink() or not package.resolve().is_relative_to(source):
            raise ValueError("Package escapes installed tree")
        manifest = json.loads((package / "package.json").read_text())
        if manifest.get("version") != entry.get("version"):
            raise ValueError("Installed version differs from lock: " + name)
        # Nested packages are independently selected from the lock. Keep all files,
        # browser workers, declarations, source maps and license notices in a package.
        target = destination / relative
        shutil.copytree(
            package,
            target,
            ignore=lambda directory, names: ["node_modules"] if Path(directory) == package else [],
            dirs_exist_ok=True,
        )
        copied.append(name)
    return {"mode": mode, "packages": len(copied), "omittedDevPackages": len(omitted)}
