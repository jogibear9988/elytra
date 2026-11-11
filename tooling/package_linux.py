#!/usr/bin/env python3
"""Stage a native Linux x64 Firefly app candidate with physical helpers, libraries and notices."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

from designer_shell import verify_designer_shell
from native_package import elf
from native_package.notices import cache_value, notices
from native_package.icu_data import stage_data
from native_package.payload import copy_application
from native_package.system_notices import distribution_notices

ROOT = Path(__file__).resolve().parents[1]


def helpers():
    text = (ROOT / "vendor/ladybird/Meta/CMake/ladybird_helper_processes.cmake").read_text()
    match = re.search(r"set\(ladybird_helper_processes\s+(.*?)\n\)", text, re.S)
    if not match:
        raise ValueError("Review upstream Linux helper inventory")
    names = match[1].split()
    if "WebContent" not in names or any(
        not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", name) for name in names
    ):
        raise ValueError("Invalid upstream Linux helper inventory")
    return names


def stage_plugins(build, output, binaries):
    """Copy dynamic Qt plugins if this SDK uses them; static plugin archives stay linked."""
    installed = Path(cache_value(build, "VCPKG_INSTALLED_DIR", str(build / "vcpkg_installed")))
    triplet = cache_value(build, "VCPKG_TARGET_TRIPLET")
    if not triplet or "/" in triplet or "\\" in triplet or triplet in (".", ".."):
        raise ValueError("Missing/invalid native SDK triplet")
    source = installed / triplet / "Qt6/plugins"
    count = 0
    if source.is_dir():
        for file in sorted(source.rglob("*.so")):
            target = output / "plugins" / file.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
            binaries[target] = file.resolve(strict=True)
            count += 1
    (output / "bin/qt.conf").write_text("[Paths]\nPrefix=..\nPlugins=plugins\n")
    return count


def stage(
    build,
    application,
    output,
    version,
    name,
    entry=None,
    dependencies="production",
    strip=True,
    mite_build=None,
    electron_compat=False,
):
    elf.native_tools()
    build = Path(build).resolve(strict=True)
    if mite_build is None:
        raise ValueError("A separately built standalone Mite CLI is required")
    mite_build = Path(mite_build).resolve(strict=True)
    if any(cache_value(sdk, "ENABLE_CI_BASELINE_CPU") != "ON" for sdk in [build, mite_build]):
        raise ValueError("Linux app distribution requires the portable CPU baseline in both SDKs")
    application = Path(application).resolve(strict=True)
    verify_designer_shell(application)
    output = Path(output).resolve()
    if output.exists() or output.is_symlink():
        raise ValueError("Linux package output must be fresh")
    if output.is_relative_to(application) or output.is_relative_to(build / "firefly-runtime"):
        raise ValueError("Linux package output must be outside input payloads")
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?", version):
        raise ValueError("Invalid package version")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}", name):
        raise ValueError("Invalid package name")
    if (
        cache_value(build, "CMAKE_BUILD_TYPE") != "Release"
        or cache_value(build, "FIREFLY_BUILD_RUNTIME") != "ON"
    ):
        raise ValueError("Build the actual Firefly native runtime in Release mode first")
    manifest = application / "package.json"
    package = json.loads(manifest.read_text()) if manifest.is_file() else {}
    entry = Path(entry or package.get("main", "main.mjs"))
    if entry.is_absolute() or ".." in entry.parts or not (application / entry).is_file():
        raise ValueError("Entry must be an existing application-relative file")
    source_runtime = build / "firefly-runtime"
    if (source_runtime / "bin/WebContent").is_symlink():
        raise ValueError("Expected the physical owned runtime WebContent, not an upstream alias")
    mite = mite_build / "bin/mite"
    if cache_value(mite_build, "CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("Standalone Mite must use Release mode")
    if any(
        re.search(r"qt|lagom-(web|compositing|gfx)|node|v8", name, re.I)
        for name in elf.dependencies(mite)
    ):
        raise ValueError(
            "Packaged standalone Mite must not link graphical or alternate JS runtimes"
        )
    inputs = {build / "bin/firefly": "bin/firefly"}
    for helper in helpers():
        inputs[source_runtime / "bin" / helper] = "firefly-runtime/bin/" + helper
    for source in inputs:
        elf.elf_x64(source)
    if not (source_runtime / "share/Lagom").is_dir():
        raise ValueError("Owned runtime resources are missing")
    for tool in [os.environ.get("CXX", "c++"), "strip", "dpkg-query"]:
        if not shutil.which(tool):
            raise ValueError("Missing native Linux packaging prerequisite: " + tool)
    subprocess.run(["python3", str(ROOT / "tooling/check_upstream.py")], check=True)
    if electron_compat:
        from native_package.runtime_options import verify_electron_compat

        verify_electron_compat(build / "bin/firefly")
    output.mkdir(parents=True)
    payload = copy_application(application, output / "app", dependencies)
    shutil.copytree(source_runtime / "share", output / "firefly-runtime/share", symlinks=False)
    main_icu = stage_data(build, output / "share/Lagom/icu", output)
    helper_icu = stage_data(
        build,
        output / "firefly-runtime/share/Lagom/icu",
        output,
        shared=output / main_icu["path"],
    )
    binaries = {}
    for source, relative in inputs.items():
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        binaries[target] = source.resolve(strict=True)
    plugins = stage_plugins(build, output, binaries)
    # Compile outside the payload so closure staging can prove separate originals.
    import tempfile

    with tempfile.TemporaryDirectory(prefix="firefly-linux-launcher-") as directory:
        launcher = Path(directory) / "launch"
        subprocess.run(
            [
                os.environ.get("CXX", "c++"),
                "-std=c++20",
                "-O2",
                f"-DFIREFLY_ELECTRON_COMPAT={int(electron_compat)}",
                "-DFIREFLY_ENTRY=" + json.dumps("app/" + entry.as_posix()),
                str(ROOT / "firefly/packaging/LauncherLinux.cpp"),
                "-o",
                str(launcher),
            ],
            check=True,
        )
        shutil.copy2(launcher, output / "launch")
        binaries[output / "launch"] = launcher
        # LibWebView's SDK DBus and a helper/plugin's system dependency can resolve
        # different libdbus-1.so.3 files. Declare the frontend's actual provider for
        # the package, then require all consumers' imported ABI to remain available.
        frontend = elf.dependencies(build / "bin/firefly")
        dbus = "libdbus-1.so.3"
        if dbus not in frontend:
            raise ValueError("Review Linux frontend DBus provider after SDK change")
        closure = elf.stage(output, binaries, preferred_providers={dbus: frontend[dbus]})
    if (
        any(name.startswith("libQt6") for name in closure["libraries"])
        and not (output / "plugins/platforms/libqxcb.so").is_file()
    ):
        raise ValueError("Dynamic Qt requires the packaged xcb platform plugin")
    licenses = output / "licenses"
    rust_packages = notices(licenses, build, mite_build)
    system = distribution_notices(
        licenses / "distribution", closure["libraries"], [build, mite_build]
    )
    objects = sorted(set(binaries) | {file.resolve() for file in (output / "lib").iterdir()})
    if strip:
        for file in objects:
            subprocess.run(["strip", "--strip-unneeded", str(file)], check=True)
    elf.verify(output, objects)
    for library, record in closure["libraries"].items():
        record["sha256"] = elf.digest(output / "lib" / library)
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    metadata = {
        "name": name,
        "icuData": {"mainAndLibraries": main_icu, "helpers": helper_icu},
        "version": version,
        "commit": head,
        "platform": "linux",
        "architecture": "x64",
        "entry": entry.as_posix(),
        "electronCompat": electron_compat,
        "standaloneMiteBundled": False,
        "cpuBaseline": {"firefly": "x86-64-v2", "mite": "x86-64-v2"},
        "dependencies": payload,
        "dynamicQtPlugins": plugins,
        "rustPackages": rust_packages,
        "distributionLibraries": system,
        "symbols": "stripped" if strip else "keep",
        "elf": closure,
        "status": "experimental candidate; native packaged-app acceptance required",
        "hostRequirements": [
            "compatible host glibc/loader",
            "X11 DISPLAY",
            "host graphics/audio drivers and services",
        ],
    }
    (output / "BUILD.json").write_text(json.dumps(metadata, indent=2) + "\n")
    hashes = {
        str(file.relative_to(output)): elf.digest(file)
        for file in sorted(output.rglob("*"))
        if file.is_file()
    }
    (output / "SHA256SUMS.json").write_text(json.dumps(hashes, indent=2) + "\n")
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--mite-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--name", default="firefly-app")
    parser.add_argument("--entry")
    parser.add_argument(
        "--electron-compat", action="store_true", help="Opt in to the limited Electron module alias"
    )
    parser.add_argument("--dependencies", choices=["production", "all"], default="production")
    parser.add_argument("--symbols", choices=["stripped", "keep"], default="stripped")
    parser.add_argument("--archive", action="store_true")
    args = parser.parse_args()
    archive = Path(str(args.output) + ".tar.gz")
    if args.archive and archive.exists():
        parser.error("Archive output exists")
    stage(
        args.build,
        args.app,
        args.output,
        args.version,
        args.name,
        args.entry,
        args.dependencies,
        args.symbols == "stripped",
        args.mite_build,
        args.electron_compat,
    )
    if args.archive:
        with tarfile.open(archive, "w:gz") as file:
            file.add(args.output, arcname=f"{args.name}-{args.version}-linux-x64")
        Path(str(archive) + ".sha256").write_text(elf.digest(archive) + "  " + archive.name + "\n")
    print("Staged experimental native Linux app candidate:", args.output)


if __name__ == "__main__":
    main()
