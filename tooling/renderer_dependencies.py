"""Supply pinned Linux Qt in an owned renderer build manifest, leaving upstream bytes intact."""

import json
from pathlib import Path

from firefly_dependencies import upstream_manifest_data, write_json


def manifest_data(root: Path):
    files = upstream_manifest_data(root)
    manifest = files["vcpkg.json"]
    # Upstream expects distribution Qt on Linux, whereas Ubuntu 24.04's Qt is
    # older than the required 6.10. Reuse its own version overrides/provider.
    overrides = {item["name"] for item in manifest.get("overrides", [])}
    if not {"qtbase", "qtpositioning"} <= overrides:
        raise ValueError("Review upstream Qt provider pins before building the Linux renderer")
    manifest["dependencies"].extend(
        [
            {
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
                    "widgets",
                    "xcb",
                    "xcb-xlib",
                    "xlib",
                    "xrender",
                    "xkb",
                    "xkbcommon-x11",
                    "zstd",
                ],
            },
            {"name": "qtpositioning", "platform": "linux", "default-features": False},
        ]
    )
    return files


def prepare(root: Path, destination: Path):
    destination.mkdir(parents=True, exist_ok=True)
    for name, value in manifest_data(root).items():
        write_json(destination / name, value)
    return destination


def check(root: Path, destination: Path):
    for name, value in manifest_data(root).items():
        file = destination / name
        if not file.is_file() or json.loads(file.read_text()) != value:
            raise ValueError(
                "Stale Linux renderer dependency manifest; rerun tooling/build_renderer.py"
            )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", required=True, type=Path)
    args = parser.parse_args()
    check(Path(__file__).resolve().parents[1], args.check)
    print("Pinned Linux renderer Qt manifest and upstream overlays verified")
