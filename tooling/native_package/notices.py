"""Preserve project, upstream, embedded-JS, vcpkg and Rust notices in native packages.

Shared by macOS and Linux staging. Cargo metadata and copyright files come from
already-built pinned sources; no runtime dependency downloads or OS UI occur.
"""

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]


def output(*args):
    return subprocess.check_output(list(map(str, args)), text=True)


def cache_value(build, key, default=""):
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(key + ":"):
            return line.split("=", 1)[1]
    return default


def copy_runtime_notices(destination, generated=ROOT / "mite/modules/generated"):
    """Distribute every embedded portable dependency notice, verified against its manifest."""
    manifest = json.loads((generated / "manifest.json").read_text())
    names = [name + ".LICENSE.txt" for name in manifest["bundles"]]
    for name in names:
        if Path(name).name != name or name not in manifest["files"]:
            raise ValueError("Missing or invalid runtime dependency notice")
        if hashlib.sha256((generated / name).read_bytes()).hexdigest() != manifest["files"][name]:
            raise ValueError("Changed runtime dependency notice: " + name)
    destination.mkdir(parents=True)
    for name in names + ["manifest.json"]:
        shutil.copy2(generated / name, destination / name)


def notices(destination, renderer, mite=None):
    destination.mkdir(parents=True)
    copy_runtime_notices(destination / "runtime-js")
    for name, files in {
        "sqlite3": ["copyright", "manifest.json"],
        "native-keymap": ["License.txt", "ThirdPartyNotices.txt", "manifest.json"],
    }.items():
        target = destination / "native" / name
        target.mkdir(parents=True)
        for filename in files:
            shutil.copy2(ROOT / "vendor" / name / filename, target / filename)
    shutil.copy2(ROOT / "LICENSE", destination / "Mite-Firefly-MIT.txt")
    shutil.copy2(ROOT / "vendor/ladybird/LICENSE", destination / "Ladybird-BSD-2-Clause.txt")
    shutil.copy2(ROOT / "vendor/ladybird.json", destination / "ladybird-pin.json")
    builds = [("renderer", renderer)]
    if mite is not None:
        builds.append(("mite", mite))
    for label, build in builds:
        installed = Path(cache_value(build, "VCPKG_INSTALLED_DIR", str(build / "vcpkg_installed")))
        for file in installed.glob("*/share/*/copyright"):
            target = destination / "vcpkg" / label / file.parent.name
            target.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target / "copyright")
    metadata = json.loads(
        output(
            "cargo",
            "metadata",
            "--offline",
            "--locked",
            "--format-version",
            "1",
            "--manifest-path",
            ROOT / "vendor/ladybird/Cargo.toml",
        )
    )
    inventory = []
    for package in metadata["packages"]:
        if not package.get("source"):
            continue
        directory = Path(package["manifest_path"]).parent
        target = destination / "rust" / (package["name"] + "-" + package["version"])
        target.mkdir(parents=True)
        files = [
            file
            for file in directory.iterdir()
            if file.is_file()
            and any(
                word in file.name.lower()
                for word in ["license", "licence", "copying", "notice", "copyright"]
            )
        ]
        if package.get("license_file"):
            file = directory / package["license_file"]
            if file.is_file() and file not in files:
                files.append(file)
        for file in files:
            shutil.copy2(file, target / file.name)
        shutil.copy2(directory / "Cargo.toml", target / "Cargo.toml")
        if not files:
            # Preserve the exact distributed sources/headers when a crate supplies
            # no standalone notice file, rather than inventing an author notice.
            with tarfile.open(target / "crate-source.tar.gz", "w:gz") as archive:
                archive.add(directory, arcname=directory.name)
        inventory.append(
            {key: package.get(key) for key in ["name", "version", "license", "source"]}
        )
    (destination / "rust-packages.json").write_text(json.dumps(inventory, indent=2) + "\n")
    return len(inventory)
