"""Copy built app sources with the same locked production/all dependency policy on every host."""

import json
from pathlib import Path
import shutil

from npm_payload import copy_dependencies


def copy_application(source, destination, mode="production"):
    source = Path(source).resolve(strict=True)
    destination = Path(destination)
    if mode not in ("production", "all"):
        raise ValueError("Unknown dependency mode")
    designer_lock = source / "designer-package-lock.json"
    metadata = json.loads((source / "BUILD.json").read_text()) if designer_lock.is_file() else {}
    bundled = metadata.get("webAssets") == "bundled"
    if bundled and metadata.get("dependencies", {}).get("mode") != mode:
        raise RuntimeError("Rebuild bundled Designer with the requested dependency mode")
    selections = []
    for directory, lock in [
        (source, source / "package-lock.json"),
        (source / "web", designer_lock),
    ]:
        if directory == source / "web" and bundled:
            continue
        if (directory / "node_modules").exists():
            if not lock.is_file():
                raise ValueError("Installed app dependencies require a package lock")
            selections.append((directory, lock))
    if designer_lock.is_file() and not bundled:
        if mode == "all" and metadata.get("dependencies", {}).get("mode") == "production":
            raise RuntimeError(
                "Rebuild Designer with --dependencies all before packaging all dependencies"
            )
        if not (source / "web/node_modules").is_dir():
            raise ValueError("Installed Designer dependencies are missing")

    def omit_dependencies(directory, names):
        return ["node_modules"] if any(Path(directory) == item[0] for item in selections) else []

    shutil.copytree(source, destination, ignore=omit_dependencies)
    inventory = {}
    for directory, lock in selections:
        relative = directory.relative_to(source)
        info = copy_dependencies(
            directory / "node_modules", destination / relative / "node_modules", lock, mode
        )
        inventory[str(relative)] = info
        if lock == designer_lock:
            metadata["dependencies"] = info
            (destination / "BUILD.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return {"mode": mode, "trees": inventory, "bundled": bundled}
