#!/usr/bin/env python3
"""Verify immutable macOS CI archives and their provenance before alpha publication."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

JSON_LIMIT = 1024 * 1024
PRODUCTS = {"mite", "firefly", "web-component-designer"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def decode_json(data):
    require(len(data) <= JSON_LIMIT, "Release metadata exceeds its size limit")
    value = json.loads(data, object_pairs_hook=unique_object)
    require(isinstance(value, dict), "Release metadata must be a JSON object")
    return value


def read_json(path):
    require(path.stat().st_size <= JSON_LIMIT, "Release metadata exceeds its size limit")
    return decode_json(path.read_bytes())


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def verify(assets, run, repository):
    """Bind exact unchanged archive bytes to a successful trusted snapshot workflow run."""
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository), "Invalid repository name")
    require(
        run.get("status") == "completed" and run.get("conclusion") == "success",
        "The source CI run has not completed successfully",
    )
    require(
        run.get("path") == ".github/workflows/macos.yml", "Not the native macOS package workflow"
    )
    require(run.get("event") in {"push", "workflow_dispatch"}, "Untrusted source workflow event")
    require(
        all(
            isinstance(run.get(key), dict) and run[key].get("full_name") == repository
            for key in ["repository", "head_repository"]
        ),
        "Source run repository/fork mismatch",
    )
    revision = run.get("head_sha", "")
    require(
        isinstance(revision, str) and re.fullmatch(r"[0-9a-f]{40}", revision),
        "Invalid source revision",
    )
    require(type(run.get("id")) is int and run["id"] > 0, "Invalid source run ID")
    files = {path.name: path for path in assets.iterdir()}
    require(
        len(files) == 5
        and all(path.is_file() and not path.is_symlink() for path in files.values()),
        "Expected exactly five regular release assets",
    )
    checksums_name = "SHA256SUMS-macos-arm64.json"
    sizes_name = "SIZE-macos-arm64.json"
    require(checksums_name in files and sizes_name in files, "Missing size/checksum records")
    checksums = read_json(files[checksums_name])
    require(
        isinstance(checksums, dict) and set(checksums) == set(files) - {checksums_name},
        "Checksum record does not cover exactly the published payloads",
    )
    hashes = {name: digest(path) for name, path in files.items()}
    for name, expected in checksums.items():
        require(
            isinstance(expected, str) and re.fullmatch(r"[0-9a-f]{64}", expected), "Invalid SHA-256"
        )
        require(hashes[name] == expected, f"Checksum mismatch: {name}")
    sizes = read_json(files[sizes_name])
    archives = set(files) - {sizes_name, checksums_name}
    require(
        isinstance(sizes.get("archives"), dict) and set(sizes["archives"]) == archives,
        "Archive size record mismatch",
    )
    builds = []
    products = set()
    for name in sorted(archives):
        match = re.fullmatch(
            r"(mite|firefly|web-component-designer)-v(\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?)-macos-arm64\.zip",
            name,
        )
        require(match is not None, f"Unexpected archive name: {name}")
        product, version = match.groups()
        require(product not in products, "Duplicate release product")
        products.add(product)
        require(
            type(sizes["archives"][name]) is int
            and sizes["archives"][name] == files[name].stat().st_size,
            f"Compressed archive size mismatch: {name}",
        )
        metadata_name = f"archive-{product}/BUILD.json"
        # Inspect metadata without extraction or repacking: signatures, resources,
        # symlinks and all archived bytes remain exactly as the native CI tested.
        with zipfile.ZipFile(files[name]) as archive:
            require(
                archive.namelist().count(metadata_name) == 1,
                f"Missing/ambiguous BUILD.json: {name}",
            )
            info = archive.getinfo(metadata_name)
            require(info.file_size <= JSON_LIMIT, "Archived build metadata exceeds its size limit")
            build = decode_json(archive.read(info))
        require(build.get("commit") == revision, f"Archive source revision mismatch: {name}")
        require(build.get("version") == version, f"Archive version mismatch: {name}")
        require(
            build.get("platform") == "macos" and build.get("architecture") == "arm64",
            "Unexpected native package platform",
        )
        require(
            build.get("signing") == "ad-hoc; not notarized",
            "Review the changed package signing policy",
        )
        builds.append(build)
    require(
        products == PRODUCTS and all(build == builds[0] for build in builds),
        "Mixed build provenance",
    )
    for name in ["Firefly", "Web Component Designer"]:
        require(isinstance(sizes.get(name), dict), "Missing measured bundle size")
        size = sizes[name].get("bytes")
        require(type(size) is int and size > 0, "Invalid measured bundle size")
    return {
        "repository": repository,
        "run": run["id"],
        "commit": revision,
        "version": builds[0]["version"],
        "build": builds[0],
        "sizes": sizes,
        "files": {
            name: {"bytes": path.stat().st_size, "sha256": hashes[name]}
            for name, path in sorted(files.items())
        },
    }


def verify_remote(report, release, tag):
    """Require every uploaded asset's server-reported digest before making a draft public."""
    require(
        release.get("tag_name") == tag and release.get("draft") is True,
        "Refusing unexpected or already published release",
    )
    actual = release.get("assets", [])
    require(len(actual) == len(report["files"]), "Remote asset count mismatch")
    require(
        {asset["name"] for asset in actual} == set(report["files"]), "Remote asset names mismatch"
    )
    for asset in actual:
        expected = report["files"][asset["name"]]
        require(
            asset.get("state") == "uploaded"
            and asset.get("size") == expected["bytes"]
            and asset.get("digest") == "sha256:" + expected["sha256"],
            f"Remote uploaded asset verification failed: {asset['name']}",
        )


def notes(report):
    repository = report["repository"]
    return f"""Experimental **macOS ARM64 only** alpha of Mite, Firefly and Web Component Designer.
Windows/Linux desktop packages remain incomplete. Apps are ad-hoc signed, **not notarized**.

Promoted unchanged from [successful native/package CI run {report["run"]}](https://github.com/{repository}/actions/runs/{report["run"]}),
source [{report["commit"][:12]}](https://github.com/{repository}/commit/{report["commit"]}).
Archive filenames and BUILD.json retain snapshot version **{report["version"]}**;
the alpha tag identifies this exact source, without rewriting/repacking the verified binaries.

Measured framework: {report["sizes"]["Firefly"]["bytes"] / 1024**2:.2f} MiB;
Designer app: {report["sizes"]["Web Component Designer"]["bytes"] / 1024**2:.2f} MiB uncompressed.
SIZE and SHA256SUMS records accompany all three original archives.
Public API/platform compatibility remains partial; consult the source's compatibility matrices.
No older-macOS compatibility, notarization or production security audit is claimed.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    local = commands.add_parser("verify")
    local.add_argument("--assets", type=Path, required=True)
    local.add_argument("--run-info", type=Path, required=True)
    local.add_argument("--repository", required=True)
    local.add_argument("--report", type=Path, required=True)
    local.add_argument("--notes", type=Path)
    remote = commands.add_parser("remote")
    remote.add_argument("--report", type=Path, required=True)
    remote.add_argument("--release-info", type=Path, required=True)
    remote.add_argument("--tag", required=True)
    args = parser.parse_args()
    if args.command == "verify":
        report = verify(args.assets, read_json(args.run_info), args.repository)
        args.report.write_text(json.dumps(report, indent=2) + "\n")
        if args.notes:
            args.notes.write_text(notes(report))
        print(f"PASS verified original macOS assets: {report['commit']} / {report['version']}")
    else:
        verify_remote(read_json(args.report), read_json(args.release_info), args.tag)
        print("PASS all remote draft asset sizes/states/SHA-256 digests")


if __name__ == "__main__":
    main()
