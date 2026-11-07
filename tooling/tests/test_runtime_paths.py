#!/usr/bin/env python3
"""Check the production native runtime selector against relocated and decoy filesystem layouts."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="firefly-runtime-paths-") as directory:
        root = Path(directory).resolve()
        fallback = root / "unavailable original build" / "bin"

        def selected(layout, executable):
            result = subprocess.run(
                [str(probe), layout, str(executable), str(fallback)],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
                cwd=root,
            )
            assert not result.stderr, result
            return Path(result.stdout.strip()).resolve()

        for layout in ["linux", "macos"]:
            prefix = root / f"{layout} source"
            binary = prefix / ("bin/firefly" if layout == "linux" else "Contents/MacOS/firefly")
            binary.parent.mkdir(parents=True)
            binary.touch()
            assert selected(layout, binary) == fallback
            # A neighboring upstream executable is never an owned Linux runtime.
            (binary.parent / "WebContent").touch()
            (binary.parent.parent / "Resources").mkdir()
            if layout == "linux":
                assert selected(layout, binary) == fallback
                owned = prefix / "firefly-runtime/bin"
                resources = prefix / "firefly-runtime/share/Lagom"
                owned.mkdir(parents=True)
                (owned / "WebContent").touch()
                assert selected(layout, binary) == fallback
                resources.mkdir(parents=True)
            else:
                owned = binary.parent
                resources = binary.parent.parent / "Resources"
            assert selected(layout, binary) == owned.resolve()
            moved = root / f"{layout} moved ünicode"
            shutil.move(prefix, moved)
            moved_binary = moved / binary.relative_to(prefix)
            expected = moved / owned.relative_to(prefix)
            assert selected(layout, moved_binary) == expected.resolve()
            # A missing renderer or resources cannot select an incomplete layout.
            (expected / "WebContent").unlink()
            assert selected(layout, moved_binary) == fallback
            (expected / "WebContent").touch()
            moved_resources = moved / resources.relative_to(prefix)
            moved_resources.rmdir()
            assert selected(layout, moved_binary) == fallback
        assert selected("linux", "") == fallback
    print("PASS native runtime relocation, Unicode/spaces, upstream decoy and incomplete layouts")
    print("This path-policy check does not prove Linux packaging or renderer execution")


if __name__ == "__main__":
    main()
