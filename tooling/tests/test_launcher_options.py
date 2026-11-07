#!/usr/bin/env python3
"""Compile actual package launchers and verify opt-in placement after relocation."""

import _support

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from native_package.runtime_options import verify_electron_compat

ROOT = Path(__file__).resolve().parents[2]


def main():
    assert sys.platform in ("darwin", "linux"), "No Windows application launcher implemented"
    compiler = shutil.which(os.environ.get("CXX", "c++"))
    assert compiler
    with tempfile.TemporaryDirectory(prefix="firefly-launcher-options-") as directory:
        root = Path(directory).resolve()
        for compatible in [False, True]:
            package = root / f"package-{compatible}"
            native = package / "Contents/MacOS" if sys.platform == "darwin" else package / "bin"
            native.mkdir(parents=True)
            entry = package / (
                "Contents/Resources/app/main.mjs" if sys.platform == "darwin" else "app/main.mjs"
            )
            entry.parent.mkdir(parents=True)
            entry.write_text("// argument fixture, not an app\n")
            launcher = native / "Launcher" if sys.platform == "darwin" else package / "launch"
            source = (
                ROOT
                / "firefly/packaging"
                / ("Launcher.cpp" if sys.platform == "darwin" else "LauncherLinux.cpp")
            )
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    f"-DFIREFLY_ELECTRON_COMPAT={int(compatible)}",
                    str(source),
                    "-o",
                    str(launcher),
                ],
                check=True,
                capture_output=True,
                timeout=30,
            )
            probe = root / "runtime.cpp"
            probe.write_text(
                '#include <cstdio>\n#include <unistd.h>\nint main(int argc,char** argv){printf("%ld\\n",(long)getpid());for(int i=1;i<argc;i++)puts(argv[i]);}\n'
            )
            subprocess.run(
                [compiler, str(probe), "-o", str(native / "firefly")],
                check=True,
                capture_output=True,
                timeout=30,
            )
            # Merely echoing the option is not proof of the native SDK contract.
            try:
                verify_electron_compat(native / "firefly")
            except ValueError:
                pass
            else:
                raise AssertionError("Unverified SDK accepted from echoed arguments")
            moved = root / f"relocated app Grüße {compatible}"
            shutil.move(package, moved)
            launcher = moved / launcher.relative_to(package)
            entry = moved / entry.relative_to(package)
            process = subprocess.Popen(
                [str(launcher), "argument with spaces", "--electron-compat"],
                cwd=root,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            stdout, stderr = process.communicate(timeout=10)
            expected = [
                str(process.pid),
                *(["--electron-compat"] if compatible else []),
                str(entry),
                "argument with spaces",
                "--electron-compat",
            ]
            assert process.returncode == 0 and stdout.splitlines() == expected and not stderr, (
                stdout,
                stderr,
                expected,
            )
        print(
            "PASS native package launcher opt-in/default argv, relocation, UTF-8 and exec PID preservation"
        )


if __name__ == "__main__":
    main()
