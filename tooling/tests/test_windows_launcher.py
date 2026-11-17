#!/usr/bin/env python3
"""Round-trip the native Windows package launcher against an instrumented child."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
PROBE = r"""
#include <filesystem>
#include <fstream>
#include <cwchar>

int wmain(int argc, wchar_t** argv)
{
    std::ofstream output(std::filesystem::path(L"received.bin"), std::ios::binary);
    for (int i = 0; i < argc; ++i)
        output.write(reinterpret_cast<char const*>(argv[i]),
            (std::wcslen(argv[i]) + 1) * sizeof(wchar_t));
    return output ? 37 : 2;
}
"""


def compile_program(source, output, *options, gui=False):
    subprocess.run(
        [
            "clang-cl",
            "/nologo",
            "/std:c++20",
            "/EHsc",
            *options,
            str(source),
            "/Fo" + str(output.with_suffix(".obj")),
            "/Fe" + str(output),
            *(["/link", "/SUBSYSTEM:WINDOWS"] if gui else []),
        ],
        check=True,
        timeout=90,
    )


def main():
    if os.name != "nt":
        raise SystemExit("Native Windows is required")

    with tempfile.TemporaryDirectory(prefix="firefly-windows-launcher-") as directory:
        root = Path(directory).resolve()
        caller = root / "unrelated working directory"
        caller.mkdir()
        recorded = caller / "received.bin"
        source = root / "probe.cpp"
        source.write_text(PROBE, encoding="ascii")
        decoy = caller / "firefly.exe"
        compile_program(source, decoy)
        launcher_source = ROOT / "firefly/packaging/LauncherWindows.cpp"

        for compatible in (False, True):
            package = root / f"package-{compatible}"
            runtime = package / "bin/firefly.exe"
            runtime.parent.mkdir(parents=True)
            shutil.copy2(decoy, runtime)
            entry = package / "app/main.mjs"
            entry.parent.mkdir()
            entry.write_text("// native launcher argument fixture\n", encoding="ascii")
            launcher = package / "Launch.exe"
            compile_program(
                launcher_source,
                launcher,
                f"/DFIREFLY_ELECTRON_COMPAT={int(compatible)}",
                "shell32.lib",
                gui=True,
            )
            moved = root / f"Relocated Grüße 中文 {compatible}"
            package.rename(moved)
            launcher = moved / launcher.name
            runtime = moved / runtime.relative_to(package)
            entry = moved / entry.relative_to(package)

            arguments = ["", "two words", 'quote"inside', 'slashes\\\\"quote', "trailing\\", "😀"]
            run = subprocess.run([str(launcher), *arguments], cwd=caller, timeout=15)
            assert run.returncode == 37, run.returncode
            expected = [str(runtime)]
            if compatible:
                expected.append("--electron-compat")
            expected.extend([str(entry), *arguments])
            assert recorded.read_bytes().decode("utf-16-le").split("\0") == [*expected, ""]
            recorded.unlink()

            # Neither a missing entry nor a missing bundled child can use the
            # caller's PATH or its same-named executable as a fallback.
            moved_entry = entry.with_suffix(".absent")
            entry.rename(moved_entry)
            run = subprocess.run([str(launcher)], cwd=caller, timeout=15)
            assert run.returncode == 1 and not recorded.exists()
            moved_entry.rename(entry)
            runtime.unlink()
            run = subprocess.run([str(launcher)], cwd=caller, timeout=15)
            assert run.returncode == 1 and not recorded.exists()

    print("PASS native Windows launcher relocation, UTF-16 arguments and child exit status")


if __name__ == "__main__":
    main()
