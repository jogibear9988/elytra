#!/usr/bin/env python3
"""Stage and relocate a real x64 EXE with normal and delay-loaded DLL imports."""

import _support

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from native_package import pe


def build(directory, name, code, *, dll=False, link=()):
    source = directory / (name + ".c")
    source.write_text(code, encoding="ascii")
    output = source.with_suffix(".dll" if dll else ".exe")
    subprocess.run(
        [
            "clang-cl",
            "/nologo",
            "/MT",
            *(["/LD"] if dll else []),
            str(source),
            "/Fo" + str(source.with_suffix(".obj")),
            "/Fe" + str(output),
            "/link",
            *map(str, link),
        ],
        cwd=directory,
        check=True,
        timeout=60,
    )
    return output


def refused(action, expected):
    try:
        action()
    except ValueError as error:
        assert expected in str(error), error
    else:
        raise AssertionError(f"Expected refusal: {expected}")


def main():
    if os.name != "nt":
        raise SystemExit("Native Windows is required")
    with tempfile.TemporaryDirectory(prefix="firefly-pe-package-") as temporary:
        root = Path(temporary).resolve()
        inputs = root / "SDK"
        inputs.mkdir()
        leaf = build(
            inputs, "Leaf", "__declspec(dllexport) int leaf(void) { return 37; }", dll=True
        )
        bridge = build(
            inputs,
            "Bridge",
            "__declspec(dllimport) int leaf(void);\n"
            "__declspec(dllexport) int bridge(void) { return leaf(); }",
            dll=True,
            link=(leaf.with_suffix(".lib"), "/DELAYLOAD:Leaf.dll", "delayimp.lib"),
        )
        image = build(
            inputs,
            "firefly",
            "__declspec(dllimport) int bridge(void);\nint main(void) { return bridge(); }",
            link=(bridge.with_suffix(".lib"),),
        )
        stage = root / "stage" / "bin"
        refused(lambda: pe.stage(stage, [image], [bridge]), "No explicit DLL provider for leaf.dll")
        assert not stage.exists()
        duplicate = root / "leaf.dll"
        shutil.copy2(leaf, duplicate)
        refused(
            lambda: pe.stage(stage, [image], [bridge, leaf, duplicate]), "duplicate DLL provider"
        )
        assert not stage.exists()
        metadata = pe.stage(stage, [image], [bridge, leaf])
        assert set(metadata["objects"]) == {"firefly.exe", "Bridge.dll", "Leaf.dll"}, metadata
        assert "bridge.dll" in metadata["objects"][image.name]["imports"]
        assert "leaf.dll" in metadata["objects"][bridge.name]["imports"]
        assert "kernel32.dll" in metadata["hostDlls"]
        moved = root / "Relocated Grüße 中文"
        stage.parent.rename(moved)
        inputs.rename(root / "SDK unavailable")
        for name, record in metadata["objects"].items():
            assert pe.digest(moved / "bin" / name) == record["sha256"]
        working = root / "unrelated cwd"
        working.mkdir()
        result = subprocess.run(
            [str(moved / "bin/firefly.exe")],
            cwd=working,
            env={**os.environ, "PATH": str(pe.system_directory())},
            timeout=15,
        )
        assert result.returncode == 37, result.returncode
    print("PASS native x64 PE closure, ordinary and delayed imports, and relocated execution")


if __name__ == "__main__":
    main()
