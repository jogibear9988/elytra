#!/usr/bin/env python3
"""Exercise the native-capability configure gate; this does not run Windows or prove confinement."""

import _support

from pathlib import Path
import subprocess
import sys
import tempfile

from firefly_dependencies import manifest_data

ROOT = Path(__file__).resolve().parents[2]


def main():
    wrong_mode = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tooling/build_renderer.py"),
            "--experimental-windows-runtime",
            "--target",
            "firefly-window-probe",
            "--configure-only",
        ],
        capture_output=True,
        text=True,
        timeout=15,
    )
    assert wrong_mode.returncode and "requires native Windows --target firefly" in wrong_mode.stderr
    manifest = manifest_data(ROOT, windows=True)["vcpkg.json"]
    names = {item if isinstance(item, str) else item["name"] for item in manifest["dependencies"]}
    assert {"libuv", "llhttp", "sqlite3"} <= names
    try:
        manifest_data(ROOT, linux=True, windows=True)
    except ValueError:
        pass
    else:
        raise AssertionError("Mixed platform selection accepted")
    with tempfile.TemporaryDirectory(prefix="firefly-windows-desktop-policy-") as directory:
        root = Path(directory)
        for name, flag, target, property_value, expected in [
            ("disabled", False, True, "TRUE", "requires explicit"),
            ("missing", True, False, None, "requires the owned native helper"),
            ("partial", True, True, "FALSE", "confinement roles incomplete"),
            ("unproven", True, True, None, "confinement roles incomplete"),
            ("complete", True, True, "TRUE", None),
        ]:
            source = root / name
            source.mkdir()
            setup = "add_library(firefly-windows-helper-launch INTERFACE)\n" if target else ""
            if property_value:
                setup += (
                    "set_property(TARGET firefly-windows-helper-launch PROPERTY "
                    f"FIREFLY_WINDOWS_HELPER_ROLES_COMPLETE {property_value})\n"
                )
            (source / "CMakeLists.txt").write_text(f"""
cmake_minimum_required(VERSION 3.30)
project(WindowsDesktopPolicy LANGUAGES NONE)
set(WIN32 TRUE)
set(FIREFLY_EXPERIMENTAL_WINDOWS_RUNTIME {"ON" if flag else "OFF"})
# An unrelated/cache boolean must never satisfy a missing target property.
set(FIREFLY_WINDOWS_HELPER_ROLES_COMPLETE TRUE CACHE BOOL "untrusted unrelated variable")
{setup}
include("{(ROOT / "firefly/native/platform/WindowsRuntime.cmake").as_posix()}")
firefly_require_windows_runtime()
""")
            result = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(source / "build")],
                capture_output=True,
                text=True,
                timeout=15,
            )
            if expected:
                assert result.returncode and expected in result.stderr, result
            else:
                assert result.returncode == 0, result
    print(
        "PASS native Windows dependency selection and explicit/full-role confinement configure gate"
    )
    print(
        "Source-policy fixtures only; native Windows runtime and package acceptance remain pending"
    )


if __name__ == "__main__":
    main()
