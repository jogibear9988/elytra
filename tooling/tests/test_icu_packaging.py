#!/usr/bin/env python3
"""Stage actual SDK ICU bytes, verify archive layouts and optionally run relocated native Mite."""

import _support

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

from native_package.icu_data import data_source, digest, stage_data


def refused(operation):
    try:
        operation()
    except ValueError:
        return
    raise AssertionError("Unsafe ICU packaging input was accepted")


def exercise_cli(binary, working_directory):
    # These converters specifically depend on data lost by upstream's browser-only
    # trim. No environment override or absolute dependency/build resource is used.
    source = """
const cases = [['big5',[0xa4,0x40]],['shift_jis',[0x82,0xa0]],
               ['euc-jp',[0xa4,0xa2]],['euc-kr',[0xb0,0xa1]]];
console.log(JSON.stringify(cases.map(([label,bytes])=>new TextDecoder(label).decode(new Uint8Array(bytes)))));
"""
    env = {
        key: value
        for key, value in os.environ.items()
        if key not in ("LADYBIRD_ICU_DATA", "ICU_DATA", "LD_LIBRARY_PATH")
        and not key.startswith("DYLD_")
    }
    result = subprocess.run(
        [str(binary), "-e", source],
        cwd=working_directory,
        env=env,
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    assert json.loads(result.stdout) == ["一", "あ", "あ", "가"], result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument(
        "--mite", type=Path, help="Actual native Mite binary for CLI relocation acceptance"
    )
    args = parser.parse_args()
    build = args.build.resolve(strict=True)
    source = data_source(build)
    original = digest(source)
    with tempfile.TemporaryDirectory(prefix="elytra-icu-package-") as temporary:
        root = Path(temporary)
        package = root / "staging"
        record = stage_data(build, package / "share/Lagom/icu", package)
        assert record["sha256"] == original and record["bytes"] == source.stat().st_size
        assert not Path(record["path"]).is_absolute()
        assert str(build) not in json.dumps(record) and str(source) not in json.dumps(record)
        assert not (package / record["path"]).is_symlink()
        again = stage_data(build, package / "share/Lagom/icu", package)
        assert again == record
        helper = stage_data(
            build,
            package / "firefly-runtime/share/Lagom/icu",
            package,
            shared=package / record["path"],
        )
        assert os.path.samefile(package / record["path"], package / helper["path"])
        assert helper["storage"] == "shared-hardlink" and helper["sha256"] == original
        bundle = package / "Example.app"
        app = stage_data(build, bundle / "Contents/Resources/icu", package)
        assert app["sha256"] == original and app["storage"] == "physical"
        assert not os.path.samefile(package / app["path"], package / record["path"])
        refused(lambda: stage_data(build, root / "escape", package))
        target = package / record["path"]
        symlink = package / "alias"
        symlink.symlink_to(target.parent, target_is_directory=True)
        refused(lambda: stage_data(build, symlink, package))
        symlink.unlink()
        invalid = root / "invalid-sdk"
        invalid.mkdir()
        (invalid / "CMakeCache.txt").write_text("LADYBIRD_ICU_DATA_FILE:INTERNAL=\n")
        refused(lambda: stage_data(invalid, root / "must-not-exist", root / "must-not-exist"))
        assert not (root / "must-not-exist").exists()
        # Negative fixtures are deliberately invalid, never synthetic positive ICU acceptance.
        malformed = invalid / source.name
        malformed.write_bytes(b"not ICU data")
        (invalid / "CMakeCache.txt").write_text(f"LADYBIRD_ICU_DATA_FILE:INTERNAL={malformed}\n")
        refused(lambda: stage_data(invalid, root / "must-not-exist", root / "must-not-exist"))
        assert not (root / "must-not-exist").exists()
        if args.mite:
            (package / "bin").mkdir()
            shutil.copy2(args.mite.resolve(strict=True), package / "bin/mite")
            (bundle / "Contents/MacOS").mkdir()
            shutil.copy2(args.mite.resolve(strict=True), bundle / "Contents/MacOS/mite")
        # Tar retains Linux's physical hardlink relation; ZIP intentionally stores
        # independent complete entries and is used for independent macOS archives.
        archive = root / "payload.tar"
        with tarfile.open(archive, "w") as file:
            file.add(package, arcname="payload")
        extracted = root / "tar relocated ünicode"
        with tarfile.open(archive) as file:
            file.extractall(extracted)
        tar_root = extracted / "payload"
        assert os.path.samefile(tar_root / record["path"], tar_root / helper["path"])
        assert digest(tar_root / record["path"]) == original
        archive = root / "mite.zip"
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as file:
            for path in package.rglob("*"):
                if path.is_file():
                    file.write(path, "archive-mite/" + path.relative_to(package).as_posix())
        extracted = root / "zip relocated ünicode"
        with zipfile.ZipFile(archive) as file:
            file.extractall(extracted)
        zip_root = extracted / "archive-mite"
        assert digest(zip_root / record["path"]) == original
        if args.mite:
            for binary in (
                tar_root / "bin/mite",
                zip_root / "bin/mite",
                zip_root / "Example.app/Contents/MacOS/mite",
            ):
                binary.chmod(0o755)
                exercise_cli(binary, root)
        # Conflicting preexisting package bytes fail rather than silently choosing
        # another SDK/version; only this owned temporary tree is modified.
        with target.open("ab") as file:
            file.write(b"changed")
        refused(lambda: stage_data(build, target.parent, package))
        assert digest(source) == original
    print(
        "PASS real ICU resource staging, hashes, independent bundle and tar/ZIP relocation layouts"
    )
    if args.mite:
        print(
            "PASS actual relocated native Mite CJK decoders without ICU/dependency environment overrides"
        )


if __name__ == "__main__":
    main()
