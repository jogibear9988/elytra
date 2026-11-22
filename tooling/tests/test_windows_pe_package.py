#!/usr/bin/env python3
"""Run relocated x64 EXEs with imports, delayed imports, a plugin and a forwarder."""

import _support

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

from native_package import pe, windows_app
from native_package.windows_archive import write_archive
from test_code_oss_sample import fixture as code_oss_fixture


def build(directory, name, code, *, dll=False, link=(), driver=()):
    source = directory / (name + ".c")
    source.write_text(code, encoding="ascii")
    output = source.with_suffix(".dll" if dll else ".exe")
    subprocess.run(
        [
            "clang-cl",
            *driver,
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
    system = pe.system_directory()
    synch = "api-ms-win-core-synch-l1-2-0.dll"
    assert pe.api_set_host(synch) == "kernelbase.dll"
    assert pe.api_set_host("api-ms-win-core-synch-l9-9-9.dll") is None
    pe.verify_api_set("api-ms-win-crt-runtime-l1-1-0.dll", system, "ucrtbase.dll")
    pe.verify_api_set(synch, system, "kernelbase.dll")
    refused(
        lambda: pe.verify_api_set(synch, system, "ucrtbase.dll"),
        "Unexpected Windows API-set provider",
    )
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
        forward_target = build(
            inputs,
            "ForwardTarget",
            "__declspec(dllexport) int forward_value(void) { return 5; }",
            dll=True,
        )
        exports = inputs / "PluginProvider.def"
        exports.write_text(
            "EXPORTS\n    plugin_value=ForwardTarget.forward_value\n", encoding="ascii"
        )
        plugin_dependency = build(
            inputs,
            "PluginProvider",
            "#include <windows.h>\nBOOL WINAPI DllMain(HINSTANCE a, DWORD b, void *c) { return TRUE; }",
            dll=True,
            link=("/DEF:" + str(exports),),
            # The hosted MSVC linker treats the forward target as a local symbol.
            driver=("-fuse-ld=lld",),
        )
        plugin = build(
            inputs,
            "PlatformFixture",
            "#include <windows.h>\n#include <dwmapi.h>\n"
            "__declspec(dllimport) int plugin_value(void);\n"
            "__declspec(dllexport) int platform_value(void) {\n"
            "    BOOL enabled = FALSE;\n"
            "    DwmIsCompositionEnabled(&enabled);\n"
            "    return plugin_value();\n"
            "}",
            dll=True,
            link=(plugin_dependency.with_suffix(".lib"), "dwmapi.lib"),
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
        refused(
            lambda: pe.stage(stage, [plugin_host], [plugin_dependency], plugins=[placement]),
            "No explicit DLL provider for forwardtarget.dll",
        )
        assert not stage.exists()
        metadata = pe.stage(
            stage,
            [image, plugin_host],
            [bridge, leaf, plugin_dependency, forward_target],
            plugins=[placement],
        )
        assert set(metadata["objects"]) == {
            "firefly.exe",
            "Bridge.dll",
            "Leaf.dll",
            "plugin-host.exe",
            "PluginProvider.dll",
            "ForwardTarget.dll",
            "platforms/PlatformFixture.dll",
        }, metadata
        assert "pluginprovider.dll" in metadata["objects"][placement[0]]["imports"]
        assert "dwmapi.dll" in metadata["objects"][placement[0]]["imports"]
        provider = metadata["objects"][plugin_dependency.name]
        assert "forwardtarget.dll" not in provider["imports"]
        assert provider["forwarders"] == ["forwardtarget.dll"], provider
        assert "bridge.dll" in metadata["objects"][image.name]["imports"]
        assert "leaf.dll" in metadata["objects"][bridge.name]["imports"]
        assert "kernel32.dll" in metadata["hostDlls"]
        assert "dwmapi.dll" in metadata["hostDlls"]
        app = root / "source app"
        app.mkdir()
        entry = app / "main.mjs"
        entry.write_text("// Test application payload.\n", encoding="utf-8")
        assembled = root / "assembled"
        refused(
            lambda: windows_app.stage(assembled, app, image, [bridge]),
            "No explicit DLL provider for leaf.dll",
        )
        assert not assembled.exists()
        resources = inputs / "resource source"
        resources.mkdir()
        resource = resources / "fixture.txt"
        resource.write_bytes(b"real helper resource fixture")
        targets = inputs / "targets.txt"
        targets.write_text(f"WebContent.exe|{image}\n{bridge}\n{leaf}\n", encoding="utf-8")
        sdk_helpers = inputs / "firefly-runtime"
        revision = "a" * 40
        subprocess.run(
            [
                sys.executable,
                str(windows_app.ROOT / "tooling/windows-sandbox/stage.py"),
                "--root",
                str(sdk_helpers),
                "--targets",
                str(targets),
                "--resources",
                str(resources),
                "--revision",
                revision,
            ],
            check=True,
            timeout=30,
        )
        manifest = windows_app.stage(
            assembled,
            app,
            image,
            [bridge, leaf],
            helper_runtime=sdk_helpers,
            helper_revision=revision,
        )
        helper_seal = "firefly-runtime/.firefly-windows-runtime-v1"
        assert manifest["helpers"]["revision"] == revision
        assert manifest["helpers"]["sealSha256"] == pe.digest(assembled / helper_seal)
        assert (assembled / helper_seal).read_bytes() == (inputs / helper_seal).read_bytes()
        assert (
            assembled / "firefly-runtime/share/Lagom/fixture.txt"
        ).read_bytes() == resource.read_bytes()
        assert set(manifest["pe"]["objects"]) == {"firefly.exe", "Bridge.dll", "Leaf.dll"}
        assert set(manifest["launcher"]["objects"]) == {"Launch.exe"}
        assert (assembled / "app/main.mjs").read_bytes() == entry.read_bytes()
        refused(
            lambda: windows_app.stage(assembled, app, image, [bridge, leaf]),
            "Windows package output must be fresh",
        )
        code_oss = root / "code-oss source"
        code_oss.mkdir()
        code_oss_metadata = code_oss_fixture(code_oss, physical=True)
        code_oss_package = root / "assembled Code-OSS"
        packaged = windows_app.stage(
            code_oss_package, code_oss, image, [bridge, leaf], code_oss=True, electron_compat=True
        )
        assert packaged["codeOSS"] == code_oss_metadata
        source = code_oss_package / "app/src/main.js"
        renderer = code_oss_package / "app/out/main.js"
        assert not renderer.is_symlink()
        assert source.read_bytes() == renderer.read_bytes()
        relocated_code_oss = root / "Relocated Code-OSS Grüße 中文"
        archive = root / "code-oss-windows-x64.zip"
        write_archive(code_oss_package, archive)
        with zipfile.ZipFile(archive) as bundled:
            assert bundled.testzip() is None
            bundled.extractall(relocated_code_oss)
        shutil.rmtree(code_oss_package)
        assert (relocated_code_oss / "app/out/main.js").read_bytes() == (
            relocated_code_oss / "app/src/main.js"
        ).read_bytes()
        relocated_app = root / "Relocated Anwendung Grüße 中文"
        assembled.rename(relocated_app)
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
        result = subprocess.run(
            [str(relocated_app / "Launch.exe")],
            cwd=working,
            env={**os.environ, "PATH": str(pe.system_directory())},
            timeout=15,
        )
        assert result.returncode == 37, result.returncode
        result = subprocess.run(
            [str(relocated_app / "firefly-runtime/bin/WebContent.exe")],
            cwd=working,
            env={**os.environ, "PATH": str(pe.system_directory())},
            timeout=15,
        )
        assert result.returncode == 37, result.returncode
        result = subprocess.run(
            [str(relocated_code_oss / "Launch.exe")],
            cwd=working,
            env={**os.environ, "PATH": str(pe.system_directory())},
            timeout=15,
        )
        assert result.returncode == 37, result.returncode
        print("PASS physical sealed Code-OSS sample ZIP and extracted native launcher fixture")
        print("PASS relocated app launcher, PE closure, and independently sealed helper fixture")
    print("PASS native x64 PE closure, delayed imports, forwarded export, plugin, and relocation")


if __name__ == "__main__":
    main()
