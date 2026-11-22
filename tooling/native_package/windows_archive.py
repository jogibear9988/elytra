"""Build ZIP archives from physical, portable Windows package trees."""

import stat
from pathlib import Path
import tempfile
import zipfile

from check_checkout_paths import reason


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
