"""Supply pinned Linux Qt in an owned renderer build manifest, leaving upstream bytes intact."""

import json
from pathlib import Path

from firefly_dependencies import upstream_manifest_data, write_json


def manifest_data(root: Path, services=False):
    files = upstream_manifest_data(root)
    manifest = files["vcpkg.json"]
    # Upstream expects distribution Qt on Linux, whereas Ubuntu 24.04's Qt is
    # older than the required 6.10. Reuse its own version overrides/provider.
    overrides = {item["name"] for item in manifest.get("overrides", [])}
    if not {"qtbase", "qtpositioning"} <= overrides:
        raise ValueError("Review upstream Qt provider pins before building the Linux renderer")
    qtbase = {
        "name": "qtbase",
        "platform": "linux",
        "default-features": False,
        "features": [
            "dbus",
            "doubleconversion",
            "fontconfig",
            "freetype",
            "gui",
            "harfbuzz",
            "icu",
            "jpeg",
            "network",
            "opengl",
            "pcre2",
            "png",
            "thread",
            "vulkan",
            "wayland",
            "widgets",
            "xcb",
            "xcb-xlib",
            "xlib",
            "xrender",
            "xkb",
            "xkbcommon-x11",
            "zstd",
        ],
    }
    if services:
        # Isolate OS-service acceptance while keeping the renderer's exact Qt
        # features, provider and overlays for binary-package reuse.
        manifest["dependencies"] = [qtbase]
    else:
        manifest["dependencies"].extend(
            [
                qtbase,
                {"name": "qtpositioning", "platform": "linux", "default-features": False},
            ]
        )
    return files


def prepare(root: Path, destination: Path, services=False):
    destination.mkdir(parents=True, exist_ok=True)
    for name, value in manifest_data(root, services).items():
        write_json(destination / name, value)
    return destination


def check(root: Path, destination: Path, services=False):
    for name, value in manifest_data(root, services).items():
        file = destination / name
        if not file.is_file() or json.loads(file.read_text()) != value:
            raise ValueError(
                "Stale Linux native dependency manifest; regenerate the selected build manifest"
            )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", type=Path)
    mode.add_argument("--prepare", type=Path)
    parser.add_argument(
        "--services", action="store_true", help="Select the standalone Qt OS-service graph"
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.prepare:
        prepare(root, args.prepare.resolve(), args.services)
    else:
        check(root, args.check, args.services)
    print("Pinned Linux native Qt manifest and upstream overlays verified")
