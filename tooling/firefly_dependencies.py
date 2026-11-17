"""Generate the browser + Mite host manifest outside the upstream checkout."""

import json
from pathlib import Path
from upstream_dependencies import upstream_manifest_data, write_json
from renderer_dependencies import manifest_data as renderer_manifest


def manifest_data(root: Path, linux=False, windows=False):
    if linux and windows:
        raise ValueError("Select exactly one native runtime dependency platform")
    files = renderer_manifest(root) if linux else upstream_manifest_data(root)
    manifest = files["vcpkg.json"]
    if windows:
        qt = [
            item
            for item in manifest["dependencies"]
            if isinstance(item, dict)
            and item.get("name") == "qtbase"
            and item.get("platform") == "windows"
        ]
        if len(qt) != 1 or not {"gui", "widgets", "windeployqt", "network"} <= set(
            qt[0].get("features", [])
        ):
            raise ValueError("Review the pinned native Windows Qt provider/features")
    mite = json.loads((root / "vcpkg.json").read_text())
    if manifest["builtin-baseline"] != mite["builtin-baseline"]:
        raise ValueError("Browser and Mite dependency baselines must match")
    names = {item if isinstance(item, str) else item["name"] for item in manifest["dependencies"]}
    # The browser already provides the shared engine dependencies. These two are
    # Mite's native I/O/protocol services, not additions to Ladybird's source.
    for name in ["libuv", "llhttp"]:
        if name not in names:
            manifest["dependencies"].append(name)
        override = next((item for item in mite.get("overrides", []) if item["name"] == name), None)
        if override:
            overrides = manifest.setdefault("overrides", [])
            existing = next((item for item in overrides if item["name"] == name), None)
            if existing and existing != override:
                raise ValueError(f"Conflicting {name} pins; resolve in our build adapter")
            if not existing:
                overrides.append(override)
    return files


def prepare(root: Path, destination: Path, linux=False, windows=False):
    files = manifest_data(root, linux, windows)
    destination.mkdir(parents=True, exist_ok=True)
    for name, value in files.items():
        write_json(destination / name, value)
    return destination


def check(root: Path, destination: Path, linux=False, windows=False):
    for name, expected in manifest_data(root, linux, windows).items():
        path = destination / name
        if not path.is_file() or json.loads(path.read_text()) != expected:
            raise ValueError(
                "Stale Firefly dependency manifest; run tooling/build_renderer.py --target firefly"
            )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", required=True, type=Path)
    parser.add_argument(
        "--linux", action="store_true", help="Require the pinned native Linux Qt graph"
    )
    parser.add_argument("--windows", action="store_true", help="Verify the native Windows Qt graph")
    args = parser.parse_args()
    check(Path(__file__).resolve().parents[1], args.check, args.linux, args.windows)
    print("Current browser + Mite dependency manifest verified")
