#!/usr/bin/env python3
"""Exercise real ELF closure staging with independently compiled native fixture programs."""

import _support

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest.mock import patch

from native_package import elf


def refused(operation, message):
    try:
        operation()
    except (ValueError, RuntimeError) as error:
        assert message in str(error), (message, error)
    else:
        raise AssertionError(f"Unsafe package input accepted: {message}")


def guards():
    """Pure guard/parser checks; no native acceptance or GUI launch is claimed."""
    with tempfile.TemporaryDirectory(prefix="elf-policy-") as directory:
        root = Path(directory).resolve()
        with patch.object(elf.sys, "platform", "darwin"), patch.object(elf, "command") as command:
            refused(lambda: elf.stage(root, {}), "requires native Linux x64")
            command.assert_not_called()
        with (
            patch.object(elf.sys, "platform", "linux"),
            patch.object(elf.platform, "machine", return_value="aarch64"),
        ):
            refused(elf.native_tools, "requires native Linux x64")
        first = root / "opaque fixture.so"
        second = root / "another opaque.so"
        first.touch()
        second.touch()
        refused(lambda: elf.elf_x64(first), "Expected a little-endian")
        listing = f"\tlibfixture.so.1 => {first} (0x123)\n"
        assert elf.parse_dependencies(listing) == {"libfixture.so.1": first}
        refused(lambda: elf.parse_dependencies("libmissing.so => not found"), "Unresolved ELF")
        refused(lambda: elf.parse_dependencies("../escape => nowhere (0x1)"), "Unrecognized ELF")
        refused(
            lambda: elf.parse_dependencies(listing + f"libfixture.so.1 => {second} (0x456)"),
            "Ambiguous ELF",
        )
        assert set(root.iterdir()) == {first, second}
    print("PASS unsupported-host/non-ELF/parser refusal; no loader, patcher or GUI launched")


def compile_graph(root, value, absolute=False):
    """Build real transitive DSOs using relative or absolute development RUNPATHs."""
    root.mkdir()
    libraries = root / "lib"
    libraries.mkdir()
    compiler = os.environ.get("CC", "cc")
    leaf = libraries / "libleaf-fixture.so.1"
    middle = libraries / "libmiddle-fixture.so.1"
    source = root / "leaf.c"
    source.write_text(f"int leaf(void) {{ return {value - 1}; }}\n")
    subprocess.run(
        [compiler, "-shared", "-fPIC", str(source), "-Wl,-soname," + leaf.name, "-o", str(leaf)],
        check=True,
    )
    source = root / "middle.c"
    source.write_text("int leaf(void); int middle(void) { return leaf() + 1; }\n")
    subprocess.run(
        [
            compiler,
            "-shared",
            "-fPIC",
            str(source),
            "-L" + str(libraries),
            "-l:" + leaf.name,
            "-Wl,-rpath," + (str(libraries) if absolute else "$ORIGIN"),
            "-Wl,-soname," + middle.name,
            "-o",
            str(middle),
        ],
        check=True,
    )
    source = root / "main.c"
    source.write_text(
        f'#include <stdio.h>\nint middle(void); int main(void) {{ printf("%d\\n", middle()); return middle() != {value}; }}\n'
    )
    (root / "bin").mkdir()
    binary = root / "bin/program"
    subprocess.run(
        [
            compiler,
            str(source),
            "-L" + str(libraries),
            "-l:" + middle.name,
            "-Wl,-rpath," + (str(libraries) if absolute else "$ORIGIN/../lib"),
            "-Wl,-rpath-link," + str(libraries),
            "-o",
            str(binary),
        ],
        check=True,
    )
    return binary


def copied(root, source, relative):
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    return target


def native():
    elf.native_tools()
    with tempfile.TemporaryDirectory(prefix="elf-native-") as directory:
        root = Path(directory).resolve()
        original = root / "original build"
        binary = compile_graph(original, 17)
        before = {file: elf.digest(file) for file in original.rglob("*") if file.is_file()}
        package = root / "staged ünicode"
        main = copied(package, binary, "bin/app")
        helper = copied(package, binary, "firefly-runtime/bin/helper")
        report = elf.stage(package, {main: binary, helper: binary})
        assert report["physicalLibraries"] == 2 and set(report["libraries"]) == {
            "libleaf-fixture.so.1",
            "libmiddle-fixture.so.1",
        }, report
        assert all(elf.digest(file) == checksum for file, checksum in before.items()), (
            "source build was modified"
        )
        assert "libc.so.6" in report["hostABI"] and not set(report["libraries"]) & elf.HOST_ABI, (
            report
        )
        moved = root / "relocated package ü"
        package.rename(moved)
        # These source paths really disappear; a leaked absolute RUNPATH cannot pass.
        original.rename(root / "unavailable source build")
        objects = [
            moved / "bin/app",
            moved / "firefly-runtime/bin/helper",
            *sorted((moved / "lib").iterdir()),
        ]
        elf.verify(moved, objects)
        env = {key: value for key, value in os.environ.items() if not key.startswith("LD_")}
        for executable in objects[:2]:
            result = subprocess.run(
                [str(executable)], env=env, capture_output=True, text=True, check=True, timeout=5
            )
            assert result.stdout == "17\n" and not result.stderr, result
        for name, value in report["libraries"].items():
            assert elf.digest(moved / "lib" / name) == value["sha256"], value
        (moved / "lib/libleaf-fixture.so.1").unlink()
        refused(lambda: elf.verify(moved, objects[:2]), "Unresolved ELF")

        left = compile_graph(root / "left build", 23, absolute=True)
        right = compile_graph(root / "right build", 31)
        collision = root / "collision"
        first = copied(collision, left, "bin/first")
        second = copied(collision, right, "bin/second")
        checksums = [elf.digest(first), elf.digest(second)]
        refused(
            lambda: elf.stage(collision, {first: left, second: right}), "Conflicting ELF dependency"
        )
        assert not (collision / "lib").exists()
        assert checksums == [elf.digest(first), elf.digest(second)]
        refused(lambda: elf.stage(collision, {left: left}), "physical staged file inside")
        hardlinked = root / "hardlinked"
        (hardlinked / "bin").mkdir(parents=True)
        linked = hardlinked / "bin/main"
        os.link(left, linked)
        refused(lambda: elf.stage(hardlinked, {linked: left}), "separate files outside")
        assert not (hardlinked / "lib").exists()
        mismatch = copied(root / "mismatch", left, "bin/main")
        with mismatch.open("ab") as file:
            file.write(b"tampered fixture copy")
        refused(lambda: elf.stage(root / "mismatch", {mismatch: left}), "differs from original")
        assert not (root / "mismatch/lib").exists()
        (collision / "lib").mkdir()
        refused(lambda: elf.stage(collision, {first: left}), "fresh package lib/")
    print(
        "PASS actual ELF transitive closure, shared-library deduplication and relocated main/helper execution without source tree"
    )
    print(
        "PASS missing-library, content collision, external input and existing-output refusal before mutation"
    )
    print(
        "This fixture does not establish Firefly Linux packaging, plugins, notices or platform support"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check-guard", action="store_true", help="Run only non-native refusal/parser checks"
    )
    args = parser.parse_args()
    guards()
    if not args.check_guard:
        native()


if __name__ == "__main__":
    main()
