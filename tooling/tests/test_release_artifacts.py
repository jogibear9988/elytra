#!/usr/bin/env python3
"""Owned publication-policy fixtures; these do not substitute for native package acceptance."""

import _support

import copy
import json
from pathlib import Path
import tempfile
import warnings
import zipfile

from verify_release_artifacts import decode_json, digest, verify, verify_remote, notes

REPOSITORY = "example/elytra"
REVISION = "a" * 40


def rejected(operation):
    try:
        operation()
    except (ValueError, zipfile.BadZipFile):
        return
    raise AssertionError("Unsafe publication input was accepted")


def fixture(root):
    run = {
        "id": 123,
        "status": "completed",
        "conclusion": "success",
        "path": ".github/workflows/macos.yml",
        "event": "push",
        "head_sha": REVISION,
        "repository": {"full_name": REPOSITORY},
        "head_repository": {"full_name": REPOSITORY},
    }
    build = {
        "version": "0.1.0-dev.5",
        "commit": REVISION,
        "platform": "macos",
        "architecture": "arm64",
        "signing": "ad-hoc; not notarized",
    }
    for product in ["mite", "firefly", "web-component-designer"]:
        with zipfile.ZipFile(root / f"{product}-v0.1.0-dev.5-macos-arm64.zip", "w") as archive:
            archive.writestr(f"archive-{product}/BUILD.json", json.dumps(build))
            archive.writestr(
                f"archive-{product}/licenses/NOTICE", "Synthetic test payload, not native binaries"
            )
    sizes = {
        "Firefly": {"bytes": 1000},
        "Web Component Designer": {"bytes": 2000},
        "archives": {path.name: path.stat().st_size for path in root.glob("*.zip")},
    }
    (root / "SIZE-macos-arm64.json").write_text(json.dumps(sizes))
    checksums(root)
    return run


def checksums(root):
    files = {
        path.name: digest(path) for path in root.iterdir() if not path.name.startswith("SHA256SUMS")
    }
    (root / "SHA256SUMS-macos-arm64.json").write_text(json.dumps(files))


def main():
    with tempfile.TemporaryDirectory(prefix="elytra-release-policy-") as directory:
        root = Path(directory)
        run = fixture(root)
        report = verify(root, run, REPOSITORY)
        assert report["version"] == "0.1.0-dev.5" and report["commit"] == REVISION
        assert "retain snapshot version" in notes(report)
        for key, value in [
            ("status", "in_progress"),
            ("conclusion", "failure"),
            ("event", "pull_request"),
            ("path", ".github/workflows/linux.yml"),
            ("head_sha", "b" * 40),
            ("id", "123"),
            ("repository", {"full_name": "foreign/repo"}),
            ("head_repository", {"full_name": "fork/elytra"}),
        ]:
            changed = {**run, key: value}
            rejected(lambda: verify(root, changed, REPOSITORY))
        rejected(lambda: decode_json(b'{"duplicate":1,"duplicate":2}'))
        rejected(lambda: decode_json(b"[]"))
        rejected(lambda: decode_json(b" " * (1024 * 1024 + 1)))
        extra = root / "unexpected"
        extra.write_text("unexpected")
        rejected(lambda: verify(root, run, REPOSITORY))
        extra.unlink()
        size_file = root / "SIZE-macos-arm64.json"
        size_file.write_text(size_file.read_text() + " ")
        rejected(lambda: verify(root, run, REPOSITORY))
        fixture(root)
        size_file.unlink()
        size_file.symlink_to(root / "SHA256SUMS-macos-arm64.json")
        rejected(lambda: verify(root, run, REPOSITORY))
        size_file.unlink()
        fixture(root)
        archive_path = root / "firefly-v0.1.0-dev.5-macos-arm64.zip"
        with warnings.catch_warnings(record=True) as expected:
            warnings.simplefilter("always")
            with zipfile.ZipFile(archive_path, "a") as archive:
                archive.writestr("archive-firefly/BUILD.json", "{}")
            assert expected and "Duplicate name" in str(expected[0].message)
        # Even a self-consistent replacement checksum cannot bless ambiguous provenance.
        sizes = json.loads(size_file.read_text())
        sizes["archives"][archive_path.name] = archive_path.stat().st_size
        size_file.write_text(json.dumps(sizes))
        checksums(root)
        rejected(lambda: verify(root, run, REPOSITORY))

        for key, value in [
            ("commit", "b" * 40),
            ("version", "0.1.0-dev.6"),
            ("platform", "linux"),
            ("architecture", "x64"),
            ("profile", "mixed"),
            ("signing", "unsigned"),
        ]:
            fixture(root)
            build = {
                "version": "0.1.0-dev.5",
                "commit": REVISION,
                "platform": "macos",
                "architecture": "arm64",
                "signing": "ad-hoc; not notarized",
                key: value,
            }
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("archive-firefly/BUILD.json", json.dumps(build))
            sizes = json.loads(size_file.read_text())
            sizes["archives"][archive_path.name] = archive_path.stat().st_size
            size_file.write_text(json.dumps(sizes))
            checksums(root)
            rejected(lambda: verify(root, run, REPOSITORY))

        fixture(root)
        report = verify(root, run, REPOSITORY)
        release = {
            "tag_name": "v0.1.0-alpha.2",
            "draft": True,
            "assets": [
                {
                    "name": name,
                    "size": value["bytes"],
                    "state": "uploaded",
                    "digest": "sha256:" + value["sha256"],
                }
                for name, value in report["files"].items()
            ],
        }
        verify_remote(report, release, release["tag_name"])
        for key, value in [
            ("draft", False),
            ("tag_name", "v0.1.0-alpha.1"),
            ("assets", release["assets"][:-1]),
        ]:
            changed = {**release, key: value}
            rejected(lambda: verify_remote(report, changed, release["tag_name"]))
        for key, value in [
            ("digest", None),
            ("digest", "sha256:" + "0" * 64),
            ("state", "new"),
            ("size", -1),
            ("name", "foreign"),
        ]:
            changed = copy.deepcopy(release)
            changed["assets"][0][key] = value
            rejected(lambda: verify_remote(report, changed, release["tag_name"]))
    print("PASS owned artifact provenance/checksum/metadata and remote draft-digest refusal policy")


if __name__ == "__main__":
    main()
