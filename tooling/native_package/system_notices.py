"""Preserve Debian/Ubuntu copyright records for system DSOs copied into a Linux package."""

import json
from pathlib import Path
import re
import shutil
import subprocess


def distribution_notices(destination, libraries, builds):
    destination = Path(destination)
    builds = [Path(build).resolve(strict=True) for build in builds]
    inventory = {}
    for name, entry in libraries.items():
        source = Path(entry["source"])
        if any(source.is_relative_to(build) for build in builds):
            continue  # Project/vcpkg/Rust notices cover this pinned build graph.
        candidates = [str(source)]
        if str(source).startswith("/usr/lib/"):
            candidates.append(str(source).removeprefix("/usr"))
        owners = set()
        for candidate in candidates:
            result = subprocess.run(
                ["dpkg-query", "--search", candidate], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                owners.update(
                    line.partition(": ")[0] for line in result.stdout.splitlines() if ": " in line
                )
                break
        if not owners:
            raise ValueError(f"No distribution copyright provenance for copied DSO: {source}")
        inventory[name] = []
        for owner in sorted(owners):
            if not re.fullmatch(r"[a-z0-9][a-z0-9+.-]*(?::[a-z0-9-]+)?", owner):
                raise ValueError(f"Ambiguous distribution package owner: {owner}")
            copyright_file = Path("/usr/share/doc") / owner.split(":")[0] / "copyright"
            if not copyright_file.is_file():
                raise ValueError(f"Distribution copyright file missing: {owner}")
            target = destination / owner.replace(":", "_")
            target.mkdir(parents=True, exist_ok=True)
            shutil.copy2(copyright_file, target / "copyright")
            version = subprocess.check_output(
                ["dpkg-query", "-W", "-f=${Version}", owner], text=True, timeout=10
            )
            inventory[name].append({"package": owner, "version": version})
    if inventory:
        shutil.copytree(
            "/usr/share/common-licenses", destination / "common-licenses", symlinks=False
        )
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "packages.json").write_text(json.dumps(inventory, indent=2) + "\n")
    return inventory
