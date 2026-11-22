"""Backport the upstream dav1d MSVC compatibility fix into an owned vcpkg port."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DAV1D_PATCH = ROOT / "firefly/cmake/patches/dav1d-msvc-has-include-next.patch"
PORTFILE_HASH = "2d91338484279a822d2415974bfc11bb014fbe436ed782e778779f4b94b90250"


def prepare(vcpkg, build):
    source = vcpkg / "ports/dav1d"
    manifest = (source / "vcpkg.json").read_bytes()
    recipe = (source / "portfile.cmake").read_text(encoding="utf-8")
    if (
        json.loads(manifest)["version"] != "1.5.4"
        or hashlib.sha256(recipe.encode()).hexdigest() != PORTFILE_HASH
    ):
        raise ValueError("Review the Windows dav1d compatibility overlay after updating vcpkg")

    # The caller has already verified the vcpkg revision. Reuse its recipe,
    # manifest and usage verbatim apart from supplying the upstream patch.
    overlay = build / "windows-ports/dav1d"
    overlay.mkdir(parents=True, exist_ok=True)
    for name, content in {
        "vcpkg.json": manifest,
        "portfile.cmake": recipe.replace(
            "    HEAD_REF master\n",
            f"    HEAD_REF master\n    PATCHES {DAV1D_PATCH.name}\n",
            1,
        ).encode(),
        "usage": (source / "usage").read_bytes(),
        DAV1D_PATCH.name: DAV1D_PATCH.read_bytes(),
    }.items():
        target = overlay / name
        if not target.is_file() or target.read_bytes() != content:
            target.write_bytes(content)
    return overlay
