#!/usr/bin/env python3
"""Validate owned renderer dependency/layout adapters; this does not claim a native Linux build."""

import _support  # Standalone tooling imports.

import json
from pathlib import Path
import subprocess
import tempfile

from firefly_dependencies import manifest_data as runtime_manifest
from renderer_dependencies import prepare, check

ROOT = Path(__file__).resolve().parents[2]


def main():
    upstream = ROOT / "vendor/ladybird"
    originals = {
        name: (upstream / name).read_bytes() for name in ["vcpkg.json", "vcpkg-configuration.json"]
    }
    with tempfile.TemporaryDirectory(prefix="firefly-renderer-platforms-") as directory:
        temporary = Path(directory)
        destination = prepare(ROOT, temporary / "manifest")
        check(ROOT, destination)
        manifest = json.loads((destination / "vcpkg.json").read_text())
        original = json.loads(originals["vcpkg.json"])
        assert manifest["builtin-baseline"] == original["builtin-baseline"]
        assert manifest["overrides"] == original["overrides"], (
            "Provider versions must stay pinned upstream"
        )
        assert manifest["dependencies"][:-2] == original["dependencies"], (
            "Do not rewrite upstream dependencies"
        )
        added = manifest["dependencies"][-2:]
        assert [item["name"] for item in added] == ["qtbase", "qtpositioning"]
        assert all(
            item["platform"] == "linux" and item["default-features"] is False for item in added
        )
        assert {"gui", "widgets", "xcb", "fontconfig"} <= set(added[0]["features"])
        configuration = json.loads((destination / "vcpkg-configuration.json").read_text())
        assert all(Path(path).is_absolute() for path in configuration["overlay-ports"])
        assert all((upstream / name).read_bytes() == value for name, value in originals.items()), (
            "Upstream was modified"
        )
        runtime = runtime_manifest(ROOT)["vcpkg.json"]
        names = {
            item if isinstance(item, str) else item["name"] for item in runtime["dependencies"]
        }
        assert {"libuv", "llhttp"} <= names
        manifest["builtin-baseline"] = "tampered"
        (destination / "vcpkg.json").write_text(json.dumps(manifest))
        try:
            check(ROOT, destination)
        except ValueError:
            pass
        else:
            raise AssertionError("Modified generated renderer dependencies were accepted")
        print(
            "PASS Linux Qt provider pins, original dependency/overlay preservation and tamper refusal"
        )

        for system in ["Darwin", "Linux", "Windows"]:
            output = temporary / f"{system}.txt"
            script = temporary / f"{system}.cmake"
            script.write_text(f'''
set(APPLE {"TRUE" if system == "Darwin" else "FALSE"})
set(CMAKE_SYSTEM_NAME "{system}")
set(CMAKE_BINARY_DIR "{temporary.as_posix()}")
set(LADYBIRD_SOURCE_DIR "{upstream.as_posix()}")
include("{(ROOT / "firefly/renderer-helper/Layout.cmake").as_posix()}")
file(WRITE "{output.as_posix()}" "${{firefly_helper_binary_dir}}\n${{firefly_helper_resources}}\n${{firefly_resource_source}}\n${{firefly_renderer_platform_sources}}\n")
''')
            result = subprocess.run(
                ["cmake", "-P", str(script)], capture_output=True, text=True, timeout=15
            )
            if system == "Windows":
                assert (
                    result.returncode
                    and "No verified native helper layout/sandbox adapter" in result.stderr
                ), result
                continue
            assert result.returncode == 0, result
            binary, resources, source, platform = output.read_text().splitlines()
            if system == "Linux":
                assert binary.endswith("/firefly-helper/bin") and resources.endswith(
                    "/firefly-helper/share/Lagom"
                )
                assert source == temporary.as_posix() + "/share/Lagom"
                assert platform.split(";") == [
                    str(upstream / "Services/RendererSandboxLinux.cpp").replace("\\", "/"),
                    str(upstream / "Services/LeakSanitizer.cpp").replace("\\", "/"),
                ]
            else:
                assert binary.endswith("/FireflyHelper.app/Contents/MacOS")
                assert resources.endswith("/FireflyHelper.app/Contents/Resources")
                assert source == "$<TARGET_FILE_DIR:ladybird>/../Resources"
                assert platform.endswith("/Services/RendererSandboxMacOS.cpp")
        print(
            "PASS actual CMake Linux/macOS resource and sandbox selection; unsupported Windows refuses"
        )


if __name__ == "__main__":
    main()
