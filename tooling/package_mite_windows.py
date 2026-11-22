#!/usr/bin/env python3
"""Build and test a standalone Mite ZIP from native Windows Release outputs."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

from native_package import pe
from native_package.icu_data import stage_data
from native_package.notices import cache_value, notices
from native_package.runtime_options import verify_libjs_revision
from native_package.windows_archive import package_files, write_archive

ROOT = Path(__file__).resolve().parents[1]
DESKTOP_LIBRARY = re.compile(r"qt\d|lagom-(?:gfx|composit|web)|firefly|v8|node\.dll", re.I)


def dll_providers(build):
    installed = Path(cache_value(build, "VCPKG_INSTALLED_DIR", str(build / "vcpkg_installed")))
    triplet = cache_value(build, "VCPKG_TARGET_TRIPLET")
    if not re.fullmatch(r"x64-windows(?:-[a-z0-9-]+)?", triplet):
        raise ValueError("Expected a native Windows x64 dependency triplet")
    candidates = [
        *sorted((build / "bin").glob("*.dll")),
        *sorted((installed / triplet / "bin").glob("*.dll")),
    ]
    redist = os.environ.get("VCToolsRedistDir")
    if redist:
        candidates.extend(sorted((Path(redist) / "x64").glob("Microsoft.VC*.CRT/*.dll")))

    by_name = {}
    for source in candidates:
        key = source.name.lower()
        if key in pe.HOST_DLLS or pe.UCRT_CONTRACT.fullmatch(key):
            continue
        pe.physical_file(source)
        previous = by_name.setdefault(key, source)
        if previous != source and pe.digest(previous) != pe.digest(source):
            raise ValueError(f"Conflicting native Mite DLL providers: {previous} and {source}")
    return by_name.values()


def verify_extracted(archive, directory, revision):
    with zipfile.ZipFile(archive) as bundle:
        # Archive entries were written from the physical package inventory.
        bundle.extractall(directory)
    runtime = directory / "bin/mite.exe"
    system = pe.system_directory()
    verify_libjs_revision(runtime, revision, search_path=system, cwd=directory.parent)
    environment = {
        key: value
        for key, value in os.environ.items()
        if key.upper() not in {"PATH", "LADYBIRD_ICU_DATA"}
        and not key.upper().startswith(("MITE_", "FIREFLY_"))
    }
    environment["PATH"] = str(system)
    expression = "new TextDecoder('windows-1252').decode(Uint8Array.of(128)).codePointAt(0)"
    result = subprocess.check_output(
        [str(runtime), "-p", expression], cwd=directory.parent, env=environment, timeout=15
    )
    if result.splitlines() != [b"8364"]:
        raise RuntimeError("Extracted Mite did not load the expected ICU converter data")
    recorded = json.loads((directory / "SHA256SUMS.json").read_text(encoding="utf-8"))
    actual = {
        file.relative_to(directory).as_posix(): pe.digest(file)
        for file in package_files(directory)
        if file != directory / "SHA256SUMS.json"
    }
    if actual != recorded:
        raise ValueError("Extracted Mite bytes differ from the release inventory")


def package_mite(build, archive, version):
    if sys.platform != "win32":
        raise RuntimeError("Mite packaging requires native Windows")
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?", version):
        raise ValueError("Invalid package version")
    if archive.exists() or archive.is_symlink():
        raise ValueError("Mite archive output must be fresh")
    source = cache_value(build, "CMAKE_HOME_DIRECTORY")
    if (
        not source
        or Path(source).resolve() != ROOT
        or cache_value(build, "CMAKE_BUILD_TYPE") != "Release"
        or cache_value(build, "BUILD_SHARED_LIBS") != "OFF"
    ):
        raise ValueError("Build the standalone Mite Release SDK before packaging")
    subprocess.run([sys.executable, str(ROOT / "tooling/check_upstream.py")], check=True, cwd=ROOT)
    pin = json.loads((ROOT / "vendor/ladybird.json").read_text())

    archive.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".mite-archive-", dir=archive.parent) as scratch:
        root = Path(scratch).resolve()
        tree = root / "package"
        closure = pe.stage(tree / "bin", [build / "bin/mite.exe"], dll_providers(build))
        if any(DESKTOP_LIBRARY.search(name) for name in closure["objects"]):
            raise ValueError("Standalone Mite has a forbidden desktop or alternate engine DLL")
        metadata = {
            "version": version,
            "platform": "windows",
            "architecture": "x64",
            "kind": "standalone-mite",
            "commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
            ).strip(),
            "ladybird": pin,
            "pe": closure,
            "rustPackages": notices(tree / "licenses", mite=build),
        }
        if cache_value(build, "LADYBIRD_ICU_DATA_FILE"):
            metadata["icuData"] = stage_data(build, tree / "share/Lagom/icu", tree)
        (tree / "BUILD.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        hashes = {
            file.relative_to(tree).as_posix(): pe.digest(file) for file in package_files(tree)
        }
        (tree / "SHA256SUMS.json").write_text(json.dumps(hashes, indent=2) + "\n", encoding="utf-8")
        candidate = root / "mite.zip"
        write_archive(tree, candidate)
        shutil.rmtree(tree)
        verify_extracted(candidate, root / "Relocated Mite ü", pin["revision"])
        candidate.rename(archive)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="Destination ZIP")
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    package_mite(args.build.resolve(strict=True), args.output.absolute(), args.version)
    print("Verified standalone Windows Mite ZIP:", args.output)


if __name__ == "__main__":
    main()
