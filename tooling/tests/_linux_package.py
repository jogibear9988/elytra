"""Stage/extract real Linux app candidates and inspect their native loader provenance."""

import json
from pathlib import Path
import subprocess
import sys
import tarfile

from native_package import elf
from test_renderer import ROOT


def packaged_app(build, mite_build, application, temporary, artifacts, name):
    source = temporary / "package-stage"
    result = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tooling/package_linux.py"),
            "--build",
            str(build),
            "--mite-build",
            str(mite_build),
            "--app",
            str(application),
            "--output",
            str(source),
            "--version",
            "0.1.0-dev.0",
            "--name",
            name,
            "--archive",
        ],
        capture_output=True,
        text=True,
        timeout=300,
    )
    (artifacts / "package-staging.log").write_text(result.stdout + "\nSTDERR:\n" + result.stderr)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    archive = Path(str(source) + ".tar.gz")
    checksum = Path(str(archive) + ".sha256").read_text().split()[0]
    assert elf.digest(archive) == checksum, "package archive digest differs"
    relocated = temporary / "archive extracted ünicode"
    with tarfile.open(archive, "r:gz") as package:
        package.extractall(relocated, filter="data")
    # The extracted archive, never its staging tree, is the application under test.
    source.rename(temporary / "unused staging tree")
    root = relocated / f"{name}-0.1.0-dev.0-linux-x64"
    sums = json.loads((root / "SHA256SUMS.json").read_text())
    actual = {
        str(path.relative_to(root))
        for path in root.rglob("*")
        if path.is_file() and path.relative_to(root).as_posix() != "SHA256SUMS.json"
    }
    assert set(sums) == actual, "package file inventory differs"
    for relative, expected in sums.items():
        path = root / relative
        assert path.resolve().is_relative_to(root.resolve()), path
        assert elf.digest(path) == expected, path
    metadata = json.loads((root / "BUILD.json").read_text())
    assert metadata["symbols"] == "stripped" and metadata["standaloneMiteBundled"] is False, (
        metadata
    )
    objects = [root / relative for relative in metadata["elf"]["binaries"]]
    objects.extend((root / "lib" / name).resolve() for name in metadata["elf"]["libraries"])
    elf.verify(root, objects)
    (artifacts / "package-build.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return root, archive


def mapped_elfs(pid, forbidden_roots):
    """The peer checks live processes, including any late-loaded SDK/plugin ELFs."""
    observed = set()
    for line in (Path("/proc") / str(pid) / "maps").read_text().splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) != 6 or not fields[5].startswith("/"):
            continue
        path = Path(fields[5])
        if not path.is_file():
            continue
        with path.open("rb") as file:
            if file.read(4) != b"\x7fELF":
                continue
        resolved = path.resolve()
        assert not any(resolved.is_relative_to(root) for root in forbidden_roots), (
            "native process loaded an ELF from the original build tree",
            pid,
            resolved,
        )
        observed.add(str(resolved))
    assert observed, ("No live native ELF mappings", pid)
    return sorted(observed)
