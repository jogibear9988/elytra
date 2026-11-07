#!/usr/bin/env python3
"""Check the real Linux launcher with a compiled argument/PID probe; this is not app acceptance."""

import _support

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from native_package.elf import HOST_ABI, dependencies, native_tools
from native_package.system_notices import distribution_notices
from test_renderer import ROOT


def main():
    native_tools()
    with tempfile.TemporaryDirectory(prefix="linux-launcher-") as directory:
        root = Path(directory).resolve()
        package = root / "package"
        (package / "bin").mkdir(parents=True)
        (package / "app").mkdir()
        (package / "app/main.mjs").write_text("// synthetic argument fixture, not an app\n")
        subprocess.run(
            [
                os.environ.get("CXX", "c++"),
                "-std=c++20",
                "-O2",
                str(ROOT / "firefly/packaging/LauncherLinux.cpp"),
                "-o",
                str(package / "launch"),
            ],
            check=True,
        )
        source = root / "runtime.c"
        source.write_text(r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[2], "argument with spaces") != 0) return 2;
    if (access(argv[1], R_OK) != 0 || !strstr(argv[1], "/app/main.mjs")) return 3;
    if (!getenv("QT_QPA_PLATFORM") || strcmp(getenv("QT_QPA_PLATFORM"), getenv("EXPECTED_PLATFORM")) != 0) return 4;
    printf("%ld\n", (long)getpid());
    return 0;
}
""")
        subprocess.run(
            [os.environ.get("CC", "cc"), str(source), "-o", str(package / "bin/firefly")],
            check=True,
        )
        # Collect real distro copyright/version provenance for the launcher's
        # non-glibc DSOs; fixture-specific invented notices cannot satisfy this.
        libraries = {
            name: {"source": str(path)}
            for name, path in dependencies(package / "launch").items()
            if name not in HOST_ABI
        }
        assert libraries, "Expected actual C++ runtime dependencies"
        inventory = distribution_notices(root / "licenses", libraries, [])
        assert set(inventory) == set(libraries) and all(inventory.values()), inventory
        for owners in inventory.values():
            for owner in owners:
                assert owner["version"]
                assert (
                    root / "licenses" / owner["package"].replace(":", "_") / "copyright"
                ).is_file()
        moved = root / "relocated ünicode"
        package.rename(moved)
        for platform in [None, "wayland"]:
            env = {
                key: value
                for key, value in os.environ.items()
                if key != "QT_QPA_PLATFORM" and not key.startswith("LD_")
            }
            env["EXPECTED_PLATFORM"] = platform or "xcb"
            if platform:
                env["QT_QPA_PLATFORM"] = platform
            process = subprocess.Popen(
                [str(moved / "launch"), "argument with spaces"],
                cwd=root,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            stdout, stderr = process.communicate(timeout=5)
            assert process.returncode == 0 and stdout == f"{process.pid}\n" and not stderr, (
                process.returncode,
                stdout,
                stderr,
            )
        (moved / "bin/firefly").unlink()
        result = subprocess.run(
            [str(moved / "launch")], cwd=root, capture_output=True, text=True, timeout=5
        )
        assert result.returncode == 1 and "Cannot launch packaged Firefly" in result.stderr, result
    print(
        "PASS actual native launcher relocation, entry/argument forwarding, exec PID ownership, X11 default, caller choice and actual distro notices"
    )
    print("The compiled probe validates launcher mechanics, not Firefly/Designer behavior")


if __name__ == "__main__":
    main()
