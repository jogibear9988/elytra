"""Read pinned upstream dependency inputs without changing files in its checkout."""

import json
from pathlib import Path


def write_json(path, value):
    text = json.dumps(value, indent=2) + "\n"
    if not path.is_file() or path.read_text() != text:
        path.write_text(text)


def upstream_manifest_data(root: Path):
    """Resolve owned-build overlay paths while retaining every upstream dependency and pin."""
    upstream = root / "vendor/ladybird"
    manifest = json.loads((upstream / "vcpkg.json").read_text())
    configuration = json.loads((upstream / "vcpkg-configuration.json").read_text())
    for key in ["overlay-ports", "overlay-triplets"]:
        if key in configuration:
            configuration[key] = [str((upstream / item).resolve()) for item in configuration[key]]
    return {"vcpkg.json": manifest, "vcpkg-configuration.json": configuration}
