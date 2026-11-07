"""Require independently observed Windows helper ownership and record native token facts."""

import json
from pathlib import Path
import re
import subprocess
import tempfile
import time

from _windows_processes import WindowsProcesses


def observe(probe, artifacts, environment, stop, page):
    native = WindowsProcesses()
    expected = {
        name: (probe.parent / f"{name}.exe").resolve(strict=True)
        for name in ("WebContent", "RequestServer", "Compositor")
    }
    observations = {}
    identities = {name: set() for name in expected}
    stdout, stderr = "", ""
    parent = None
    with tempfile.TemporaryDirectory(prefix="firefly-win-processes-") as directory:
        process = subprocess.Popen(
            [str(probe), "--headless=manual", page],
            env=environment(directory),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            parent = native.inspect(process.pid)
            assert Path(parent["image"]) == probe, parent
            deadline = time.monotonic() + 20
            while process.poll() is None and time.monotonic() < deadline:
                for pid, executable in native.children(process.pid).items():
                    name = next(
                        (
                            name
                            for name in expected
                            if executable.casefold() == f"{name}.exe".casefold()
                        ),
                        None,
                    )
                    if name is None:
                        continue
                    try:
                        child = native.inspect(pid)
                    except ProcessLookupError:
                        continue
                    assert child["pid"] != process.pid, child
                    assert Path(child["image"]) == expected[name], (child, expected[name])
                    assert child["creationTicks"] >= parent["creationTicks"], (child, parent)
                    # Independently selected by Toolhelp, not the page.
                    child["parentPid"] = process.pid
                    observations[pid] = child
                    identities[name].add(pid)
                time.sleep(0.05)
            stdout, stderr = process.communicate(timeout=15)
            renderer = re.search(r"^WebContent PID: (\d+)$", stdout, re.MULTILINE)
            assert process.returncode == 0 and renderer, (process.returncode, stdout, stderr)
            assert int(renderer[1]) in identities["WebContent"] and all(identities.values()), (
                identities,
                observations,
                stdout,
                stderr,
            )
        finally:
            stop(process)
            stdout, stderr = process.communicate(timeout=10)
            (artifacts / "windows-process-stdout.log").write_text(stdout)
            (artifacts / "windows-process-stderr.log").write_text(stderr)
            (artifacts / "windows-processes.json").write_text(
                json.dumps(
                    {
                        "main": parent,
                        "children": list(observations.values()),
                        "status": "process observations only; Windows renderer sandbox unimplemented in pinned upstream",
                    },
                    indent=2,
                )
                + "\n"
            )
    print(
        "PASS native Windows WebContent/RequestServer/Compositor executable identities, parent PIDs and creation times"
    )
    print(
        "UNVERIFIED Windows containment: pinned RendererSandboxUnimplemented applies no sandbox; token/job facts are observations only"
    )
