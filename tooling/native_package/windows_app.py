"""Assemble the launcher, application files and PE closure for a Windows package."""

import json
import os
from pathlib import Path
import subprocess
import tempfile

from native_package import pe
from native_package.payload import copy_application

LAUNCHER = Path(__file__).resolve().parents[2] / "firefly/packaging/LauncherWindows.cpp"


def stage(
    output,
    application,
    runtime,
    providers,
    *,
    plugins=(),
    dependencies="production",
    electron_compat=False,
):
    """Publish the app and main-process binaries as one relocatable directory.

    The caller supplies trusted build outputs. Helpers, their resources and
    platform notices belong to the SDK packaging layer.
    """
    if os.name != "nt":
        raise RuntimeError("Windows package assembly requires native Windows")
    if runtime.name.lower() != "firefly.exe":
        raise ValueError("Expected the Firefly executable")
    application = application.resolve(strict=True)
    if output.exists() or output.is_symlink():
        raise ValueError("Windows package output must be fresh")
    output = output.resolve()
    if output.is_relative_to(application):
        raise ValueError("Windows package output must be outside the application")
    pe.physical_file(application / "main.mjs")

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".windows-app-", dir=output.parent) as temporary:
        scratch = Path(temporary)
        package = scratch / "package"
        closure = pe.stage(package / "bin", [runtime], providers, plugins=plugins)
        (package / "bin/qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\n", encoding="ascii")
        payload = copy_application(application, package / "app", dependencies)

        # The root launcher cannot depend on the DLL search directory of its child.
        # Link the CRT statically and check the resulting host-only import closure.
        launcher = scratch / "Launch.exe"
        subprocess.run(
            [
                "clang-cl",
                "/nologo",
                "/std:c++20",
                "/EHsc",
                "/O2",
                "/MT",
                f"/DFIREFLY_ELECTRON_COMPAT={int(electron_compat)}",
                str(LAUNCHER),
                "/Fo" + str(launcher.with_suffix(".obj")),
                "/Fe" + str(launcher),
                "/link",
                "/SUBSYSTEM:WINDOWS",
                "shell32.lib",
            ],
            check=True,
            timeout=90,
        )
        launcher_closure = pe.stage(scratch / "launcher", [launcher], [])
        (scratch / "launcher/Launch.exe").rename(package / "Launch.exe")
        metadata = {
            "architecture": "x64",
            "electronCompat": electron_compat,
            "dependencies": payload,
            "launcher": launcher_closure,
            "pe": closure,
        }
        (package / "BUILD.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        package.rename(output)
    return metadata
