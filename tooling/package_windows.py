#!/usr/bin/env python3
"""Stage an experimental native Windows x64 Firefly app from a Release SDK."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

from check_checkout_paths import reason
from designer_shell import verify_designer_shell
from native_package import pe, windows_app
from native_package.icu_data import stage_data
from native_package.notices import cache_value, notices
from native_package.runtime_options import verify_libjs_revision

ROOT = Path(__file__).resolve().parents[1]
QT_PLUGINS = (
    "iconengines",
    "imageformats",
    "networkinformation",
    "platforms",
    "styles",
    "tls",
)


def package_files(package):
    """Refuse aliases and ambiguous Windows paths before writing an archive."""
    files = []
    paths = set()

    def walk(directory):
        for path in sorted(directory.iterdir()):
            relative = path.relative_to(package).as_posix()
            key = relative.casefold()
            info = path.lstat()
            if reason(relative) or key in paths:
                raise ValueError(f"Invalid or duplicate Windows package path: {relative}")
            paths.add(key)
            if stat.S_ISLNK(info.st_mode) or (
                getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
            ):
                raise ValueError(f"Windows packages require physical paths: {relative}")
            if stat.S_ISDIR(info.st_mode):
                walk(path)
            elif stat.S_ISREG(info.st_mode):
                files.append(path)
            else:
                raise ValueError(f"Unsupported Windows package entry: {relative}")

    walk(package)
    return files


def write_archive(package, archive):
    if archive.exists() or archive.is_symlink():
        raise ValueError("Archive output must be fresh")
    files = package_files(package)
    with tempfile.TemporaryDirectory(prefix=".windows-zip-", dir=archive.parent) as scratch:
        temporary = Path(scratch) / "package.zip"
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as output:
            for path in files:
                output.write(path, path.relative_to(package).as_posix())
        # On Windows a rename fails if the destination appeared while compressing.
        temporary.rename(archive)


def stage(build, application, output, version, *, code_oss=False):
    if sys.platform != "win32":
        raise RuntimeError("Application packaging requires native Windows")
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?", version):
        raise ValueError("Invalid package version")
    build = build.resolve(strict=True)
    application = application.resolve(strict=True)
    if output.exists() or output.is_symlink():
        raise ValueError("Windows package output must be fresh")
    output = output.resolve()
    if output.is_relative_to(build) or output.is_relative_to(application):
        raise ValueError("Windows package output must be outside build and application inputs")
    required = {
        "CMAKE_BUILD_TYPE": "Release",
        "FIREFLY_BUILD_RUNTIME": "ON",
        "FIREFLY_EXPERIMENTAL_WINDOWS_RUNTIME": "ON",
        "FIREFLY_EXPERIMENTAL_WINDOWS_HELPER_LAUNCH": "ON",
    }
    for option, value in required.items():
        if cache_value(build, option) != value:
            raise ValueError(f"Windows packaging requires {option}={value} in the native SDK")

    binaries = build / "bin"
    runtime = binaries / "firefly.exe"
    providers = sorted(binaries.glob("*.dll"))
    plugins = [
        (path.relative_to(binaries).as_posix(), path)
        for category in QT_PLUGINS
        for path in sorted((binaries / category).glob("*.dll"))
    ]
    if not any(relative.lower() == "platforms/qwindows.dll" for relative, _ in plugins):
        raise ValueError("Build the native SDK with the deployed qwindows platform plugin")
    subprocess.run([sys.executable, str(ROOT / "tooling/check_upstream.py")], check=True)
    pin = json.loads((ROOT / "vendor/ladybird.json").read_text())
    if not code_oss:
        verify_designer_shell(application)

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".win-package-", dir=output.parent) as scratch:
        package = Path(scratch) / "app"
        metadata = windows_app.stage(
            package,
            application,
            runtime,
            providers,
            plugins=plugins,
            code_oss=code_oss,
            electron_compat=code_oss,
            helper_runtime=build / "firefly-runtime",
            helper_revision=pin["revision"],
        )
        if cache_value(build, "LADYBIRD_ICU_DATA_FILE"):
            metadata["icuData"] = stage_data(build, package / "share/Lagom/icu", package)
        licenses = package / "licenses"
        metadata.update(
            platform="windows",
            version=version,
            ladybird=pin,
            commit=subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
            ).strip(),
            rustPackages=notices(licenses, build),
            status="experimental candidate; native packaged-app acceptance required",
        )
        if code_oss:
            shutil.copytree(package / "app/licenses", licenses / "Code-OSS")
        verify_libjs_revision(
            package / "bin/firefly.exe",
            pin["revision"],
            firefly=True,
            search_path=pe.system_directory(),
            cwd=package,
        )
        (package / "BUILD.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        hashes = {
            path.relative_to(package).as_posix(): pe.digest(path) for path in package_files(package)
        }
        (package / "SHA256SUMS.json").write_text(
            json.dumps(hashes, indent=2) + "\n", encoding="utf-8"
        )
        package.rename(output)
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path, help="Native Firefly Release SDK")
    parser.add_argument("--app", required=True, type=Path, help="Prepared production application")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--code-oss", action="store_true", help="Require a physical Code-OSS seal")
    parser.add_argument("--archive", action="store_true", help="Also write <output>.zip")
    args = parser.parse_args()
    archive = args.output.with_name(args.output.name + ".zip")
    if args.archive and (archive.exists() or archive.is_symlink()):
        parser.error("Archive output exists")
    stage(args.build, args.app, args.output, args.version, code_oss=args.code_oss)
    if args.archive:
        write_archive(args.output, archive)
    print("Staged experimental native Windows x64 app:", args.output)


if __name__ == "__main__":
    main()
