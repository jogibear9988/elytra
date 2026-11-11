"""Stage the SDK's actual external ICU data at upstream module/executable-relative paths."""

import hashlib
import os
from pathlib import Path
import re
import shutil

from .notices import cache_value


def digest(path):
    with Path(path).open("rb") as file:
        return (
            hashlib.file_digest(file, "sha256").hexdigest()
            if hasattr(hashlib, "file_digest")
            else _digest(file)
        )


def _digest(file):
    value = hashlib.sha256()
    for block in iter(lambda: file.read(1024 * 1024), b""):
        value.update(block)
    return value.hexdigest()


def data_source(build):
    """Require the selected SDK resource; never infer one from a dependency cache."""
    selected = cache_value(Path(build), "LADYBIRD_ICU_DATA_FILE")
    if not selected:
        raise ValueError("SDK has no selected LADYBIRD_ICU_DATA_FILE")
    source = Path(selected)
    if not source.is_absolute() or not re.fullmatch(r"icudt[0-9]+[bl]\.dat", source.name):
        raise ValueError("Invalid SDK ICU data path or filename")
    if not source.is_file():
        raise ValueError("Build the selected ICU data resource before packaging")
    with source.open("rb") as file:
        header = file.read(24)
    # ICU common-data packages have a UDataInfo header, magic DA27 and CmnD
    # format. This catches truncation/wrong resources, without parsing/repacking
    # or claiming the individual converter contents have been runtime-tested.
    if len(header) < 24 or header[2:4] != b"\xda\x27" or header[12:16] != b"CmnD":
        raise ValueError("Selected ICU resource is not a common-data package")
    endian = "big" if header[8] else "little"
    if header[8] not in (0, 1) or int.from_bytes(header[:2], endian) < 24:
        raise ValueError("Invalid ICU common-data header")
    if (source.name.endswith("b.dat")) != bool(header[8]):
        raise ValueError("ICU resource filename and data endianness differ")
    return source


def stage_data(build, directory, package_root, *, shared=None):
    """One resource per independent package; Linux helper aliases may share its inode."""
    source = data_source(build)
    root = Path(package_root).resolve()
    directory = Path(directory)
    if directory.is_symlink():
        raise ValueError("ICU destination must be a physical package directory")
    directory = directory.resolve()
    target = directory / source.name
    if not target.resolve().is_relative_to(root):
        raise ValueError("ICU destination must remain inside the owned package")
    expected = digest(source)
    if target.is_symlink() or (
        target.exists() and (not target.is_file() or digest(target) != expected)
    ):
        raise ValueError("Existing package ICU data differs from the selected SDK")
    if shared is not None:
        shared = Path(shared)
        if not shared.resolve().is_relative_to(root) or shared.is_symlink() or not shared.is_file():
            raise ValueError("Shared ICU data must be a physical file inside the package")
        shared = shared.resolve()
        if shared.name != source.name or digest(shared) != expected:
            raise ValueError("Shared ICU data differs from the selected SDK")
    directory.mkdir(parents=True, exist_ok=True)
    if shared is not None and shared != target:
        # Hardlinks keep both loader-relative paths physical and within the
        # package, with a single data payload. tarfile retains this relationship.
        if target.exists() and not os.path.samefile(shared, target):
            target.unlink()
        if not target.exists():
            os.link(shared, target)
    elif not target.exists():
        shutil.copy2(source, target)
    if digest(target) != expected:
        raise ValueError("Staged ICU data changed during packaging")
    return {
        "path": target.relative_to(root).as_posix(),
        "file": source.name,
        "bytes": target.stat().st_size,
        "sha256": expected,
        "storage": "shared-hardlink" if shared is not None and shared != target else "physical",
    }
