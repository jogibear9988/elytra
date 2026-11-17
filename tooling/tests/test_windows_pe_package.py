#!/usr/bin/env python3
"""Run a relocated x64 EXE with ordinary imports, delay imports and a plugin."""

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
        plugin_dependency = build(
            inputs,
            "PluginProvider",
            "__declspec(dllexport) int plugin_value(void) { return 5; }",
            dll=True,
        )
        plugin = build(
            inputs,
            "PlatformFixture",
            "__declspec(dllimport) int plugin_value(void);\n"
            "__declspec(dllexport) int platform_value(void) { return plugin_value(); }",
            dll=True,
            link=(plugin_dependency.with_suffix(".lib"),),
        )
        plugin_host = build(
            inputs,
            "plugin-host",
            r"""
#include <windows.h>
#include <wchar.h>
int main(void) {
    wchar_t path[32768];
    DWORD size = GetModuleFileNameW(NULL, path, 32768);
    if (!size || size >= 32768) return 2;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash || (slash - path) + 40 >= 32768) return 2;
    wcscpy_s(slash + 1, 32768 - (slash + 1 - path), L"platforms\\PlatformFixture.dll");
    HMODULE module = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) return 3;
    int (*query)(void) = (int (*)(void))GetProcAddress(module, "platform_value");
    int status = query ? query() : 4;
    FreeLibrary(module);
    return status;
}
""",
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
        for relative in (
            "../PlatformFixture.dll",
            "NUL/PlatformFixture.dll",
            "C:/PlatformFixture.dll",
        ):
            refused(
                lambda: pe.stage(stage, [image], [bridge, leaf], plugins=[(relative, plugin)]),
                "Invalid or duplicate PE plugin",
            )
        placement = ("platforms/PlatformFixture.dll", plugin)
        refused(
            lambda: pe.stage(stage, [plugin_host], [], plugins=[placement]),
            "No explicit DLL provider for pluginprovider.dll",
        )
        assert not stage.exists()
        metadata = pe.stage(
            stage, [image, plugin_host], [bridge, leaf, plugin_dependency], plugins=[placement]
        )
        assert set(metadata["objects"]) == {
            "firefly.exe",
            "Bridge.dll",
            "Leaf.dll",
            "plugin-host.exe",
            "PluginProvider.dll",
            "platforms/PlatformFixture.dll",
        }, metadata
        assert "pluginprovider.dll" in metadata["objects"][placement[0]]["imports"]
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
        result = subprocess.run(
            [str(moved / "bin/plugin-host.exe")],
            cwd=working,
            env={**os.environ, "PATH": str(pe.system_directory())},
            timeout=15,
        )
        assert result.returncode == 5, result.returncode
    print("PASS native x64 PE closure, delayed imports, explicit plugin, and relocated execution")


if __name__ == "__main__":
    main()
