#!/usr/bin/env python3
"""Run one real offscreen CodeOSS development-extension edit/save workflow, with owned cleanup."""

import argparse
import ctypes
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "firefly/tests/code-oss-workbench"
OWNER = "elytra-code-oss-workbench-acceptance-v1"
MAX_LOG = 16 * 1024 * 1024


@dataclass(frozen=True)
class NativeProcess:
    pid: int
    parent: int
    group: int
    session: int
    uid: int
    birth: tuple
    executable: str


class BsdInfo(ctypes.Structure):
    # Darwin SDK sys/proc_info.h PROC_PIDTBSDINFO, not command-line text identity.
    _fields_ = (
        [
            (name, ctypes.c_uint32)
            for name in (
                "flags",
                "status",
                "xstatus",
                "pid",
                "parent",
                "uid",
                "gid",
                "ruid",
                "rgid",
                "svuid",
                "svgid",
                "reserved",
            )
        ]
        + [("comm", ctypes.c_char * 16), ("name", ctypes.c_char * 32)]
        + [
            (name, ctypes.c_uint32)
            for name in ("nfiles", "group", "jobc", "device", "terminal_group")
        ]
        + [
            ("nice", ctypes.c_int32),
            ("seconds", ctypes.c_uint64),
            ("microseconds", ctypes.c_uint64),
        ]
    )


LIBPROC = ctypes.CDLL("/usr/lib/libproc.dylib") if sys.platform == "darwin" else None
if LIBPROC:
    LIBPROC.proc_pidinfo.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_uint64,
        ctypes.c_void_p,
        ctypes.c_int,
    ]
    LIBPROC.proc_pidinfo.restype = ctypes.c_int
    LIBPROC.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    LIBPROC.proc_pidpath.restype = ctypes.c_int


def native_process(pid):
    if LIBPROC:
        value = BsdInfo()
        if LIBPROC.proc_pidinfo(
            pid, 3, 0, ctypes.byref(value), ctypes.sizeof(value)
        ) != ctypes.sizeof(value):
            return None
        executable = ctypes.create_string_buffer(4096)
        if LIBPROC.proc_pidpath(pid, executable, len(executable)) <= 0:
            return None
        return NativeProcess(
            pid,
            value.parent,
            value.group,
            os.getsid(pid),
            value.uid,
            (value.seconds, value.microseconds),
            os.fsdecode(executable.value),
        )
    if sys.platform.startswith("linux"):
        try:
            directory = Path("/proc") / str(pid)
            fields = (directory / "stat").read_text().rsplit(") ", 1)[1].split()
            return NativeProcess(
                pid,
                int(fields[1]),
                int(fields[2]),
                os.getsid(pid),
                directory.stat().st_uid,
                (int(fields[19]),),
                os.readlink(directory / "exe"),
            )
        except (FileNotFoundError, ProcessLookupError):
            return None
    raise RuntimeError("Native process ownership is implemented only on macOS/Linux")


class OwnedTree:
    def __init__(self, process, executable):
        self.process = process
        self.members = {}
        self.signals = 0
        current = native_process(process.pid)
        if (
            not current
            or current.parent != os.getpid()
            or current.uid != os.getuid()
            or current.session != process.pid
            or Path(current.executable).resolve() != executable
        ):
            raise RuntimeError("Cannot authenticate the spawned native application")
        self.members[current.pid] = current

    def scan(self):
        live = set()
        for pid, previous in list(self.members.items()):
            current = native_process(pid)
            if current and current.birth == previous.birth and current.uid == previous.uid:
                self.members[pid] = (
                    current  # A proven same process may exec or change process group.
                )
                live.add(pid)
        listing = subprocess.run(
            ["/bin/ps", "-axo", "pid=,ppid="], capture_output=True, text=True, check=True, timeout=5
        )
        parents = {
            int(parts[0]): int(parts[1])
            for line in listing.stdout.splitlines()
            if len(parts := line.split()) == 2
        }
        sessions = {self.members[pid].session for pid in live}
        changed = True
        while changed:
            changed = False
            for pid, parent in parents.items():
                if pid in self.members:
                    continue
                try:
                    in_session = os.getsid(pid) in sessions
                except ProcessLookupError:
                    continue
                if parent not in live and not in_session:
                    continue
                current = native_process(pid)
                if current and current.parent == parent and current.uid == os.getuid():
                    self.members[pid] = current
                    live.add(pid)
                    sessions.add(current.session)
                    changed = True

    def alive(self):
        result = []
        for pid, previous in self.members.items():
            current = native_process(pid)
            if (
                current
                and current.birth == previous.birth
                and current.uid == previous.uid
                and current.executable == previous.executable
            ):
                result.append(pid)
        return result

    def stop(self):
        # Individual authenticated PIDs cover detached helpers in other groups;
        # an unrelated process group or a reused PID is never signalled.
        self.scan()
        for action, duration in ((signal.SIGTERM, 3), (signal.SIGKILL, 3)):
            for pid in reversed(self.alive()):
                try:
                    current = native_process(pid)
                    previous = self.members[pid]
                    if (
                        not current
                        or current.birth != previous.birth
                        or current.executable != previous.executable
                        or current.uid != previous.uid
                    ):
                        continue
                    os.kill(pid, action)
                    self.signals += 1
                except ProcessLookupError:
                    pass
            end = time.monotonic() + duration
            while time.monotonic() < end:
                self.process.poll()  # Reap our direct child before checking native liveness.
                self.scan()
                if not self.alive():
                    return
                time.sleep(0.1)
        if self.alive():
            raise RuntimeError("Authenticated application descendants remain alive")


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")
    path.chmod(0o600)


def prepare(state, stage, extension_format="esm"):
    marker = state / ".elytra-owned.json"
    if not state.exists():
        state.mkdir(mode=0o700)
    if marker.exists():
        if json.loads(marker.read_text()).get("owner") != OWNER:
            raise RuntimeError("Refusing an unowned acceptance profile")
    elif any(item.name != ".DS_Store" for item in state.iterdir()):
        raise RuntimeError("Acceptance state must be empty or carry its ownership marker")
    else:
        write_json(marker, {"owner": OWNER})
    profile = state / "profile"
    user = profile / "User"
    extension = state / "extension"
    extensions = state / "extensions"
    workspace = state / "workspace"
    for directory in (profile, user, extension, extensions, workspace):
        directory.mkdir(mode=0o700, exist_ok=True)
    settings = {
        "window.titleBarStyle": "native",
        "keyboard.touchbar.enabled": False,
        "workbench.startupEditor": "none",
        "security.workspace.trust.enabled": False,
    }
    write_json(user / "settings.json", settings)
    for name in ("extension.mjs", "extension.cjs", "commonjs-helper.cjs", "tests.mjs"):
        (extension / name).write_bytes((FIXTURE / name).read_bytes())
    manifest = json.loads((FIXTURE / "package.json").read_text())
    manifest["main"] = "./extension.cjs" if extension_format == "commonjs" else "./extension.mjs"
    write_json(extension / "package.json", manifest)
    nonce = str(uuid.uuid4())
    document = workspace / "owned-workbench.txt"
    initial = "owned-workbench-input\n"
    prefix = "edited-through-vscode-api:" + nonce + "\n"
    result = state / "result.json"
    for output in (
        result,
        state / "result.json.tmp",
        state / "report.json",
        state / "processes.json",
    ):
        output.unlink(missing_ok=True)
    document.write_text(initial)
    config = {
        "schema": 1,
        "extensionFormat": extension_format,
        "nonce": nonce,
        "document": str(document),
        "initial": initial,
        "prefix": prefix,
        "expected": prefix + initial,
        "result": str(result),
    }
    write_json(extension / "acceptance.json", config)
    command = [
        str(stage / "src/main.js"),
        "--user-data-dir",
        str(profile),
        "--extensions-dir",
        str(extensions),
        "--extensionDevelopmentPath",
        str(extension),
        "--extensionTestsPath",
        str(extension / "tests.mjs"),
        "--new-window",
        "--skip-welcome",
        "--disable-crash-reporter",
        "--disable-workspace-trust",
        "--log",
        "trace",
        str(workspace),
    ]
    return command, config


def redact(raw, destination):
    temporary = destination.with_suffix(".sanitizing")
    forbidden = (
        "getunixshellenvironment",
        "workbench#open()",
        "process.env",
        '"env":',
        '"environment":',
        "shellenvironment",
        "authorization:",
        "access_token",
        "password",
        "initdata",
    )
    with raw.open("r", errors="replace") as source, temporary.open("x") as output:
        os.chmod(temporary, 0o600)
        for line in source:
            if len(line) > 65536 or any(value in line.lower() for value in forbidden):
                output.write("[redacted application environment/configuration record]\n")
            else:
                output.write(line)
    os.replace(temporary, destination)
    raw.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument(
        "--state-directory", type=Path, default=Path("/tmp/ef-workbench-acceptance")
    )
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--extension-format", choices=("esm", "commonjs"), default="esm")
    args = parser.parse_args()
    if os.name != "posix" or not (sys.platform == "darwin" or sys.platform.startswith("linux")):
        parser.error("Native descendant authentication is not implemented on this platform")
    if not 30 <= args.timeout <= 300:
        parser.error("--timeout must be 30..300 seconds")
    binary = args.firefly.resolve(strict=True)
    stage = args.stage.resolve(strict=True)
    if not (stage / "src/main.js").is_file() or not (stage / "ELYTRA-COMPILE.json").is_file():
        parser.error("Use the existing prepared CodeOSS stage")
    state = args.state_directory.resolve()
    if sys.platform == "darwin" and len(os.fsencode(state / "profile")) > 60:
        parser.error("Use a short /tmp state directory for native Unix socket paths")
    state.mkdir(mode=0o700, exist_ok=True)
    lock = state / ".running"
    descriptor = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    os.write(descriptor, str(os.getpid()).encode())
    os.close(descriptor)
    retain_lock = False
    try:
        # The lock is ours, not evidence of unrelated pre-existing profile data.
        marker = state / ".elytra-owned.json"
        if not marker.exists() and any(
            item.name not in (".running", ".DS_Store") for item in state.iterdir()
        ):
            raise RuntimeError("Refusing an unowned existing profile")
        if not marker.exists():
            write_json(marker, {"owner": OWNER})
        arguments, config = prepare(state, stage, args.extension_format)
        command = [str(binary), "--electron-compat", *arguments]
        if args.prepare_only:
            print(
                json.dumps(
                    {
                        "prepared": True,
                        "command": command,
                        "stateDirectory": str(state),
                        "launched": False,
                    }
                )
            )
            return
        environment = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("VSCODE_", "ELECTRON_"))
        }
        environment.update(
            VSCODE_DEV="1",
            ELYTRA_CODE_OSS_LOCAL_DESKTOP="1",
            ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC="1",
            QT_QPA_PLATFORM="offscreen",
            XDG_CONFIG_HOME=str(state / "xdg"),
            XDG_DATA_HOME=str(state / "xdg"),
            XDG_CACHE_HOME=str(state / "xdg"),
        )
        raw = state / "run.private.log"
        raw.unlink(missing_ok=True)
        failure = None
        tree = None
        process = None
        natural_exit = None
        with raw.open("xb") as output:
            os.chmod(raw, 0o600)
            try:
                process = subprocess.Popen(
                    command,
                    cwd=stage,
                    env=environment,
                    stdout=output,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
                tree = OwnedTree(process, binary)
                end = time.monotonic() + args.timeout
                while process.poll() is None:
                    tree.scan()
                    if raw.stat().st_size > MAX_LOG:
                        raise RuntimeError("Application diagnostic log exceeded 16 MiB")
                    if time.monotonic() >= end:
                        raise RuntimeError("CodeOSS extension workflow timed out")
                    time.sleep(0.2)
                natural_exit = process.returncode
                tree.scan()
            except Exception as error:
                failure = error
            finally:
                if tree:
                    try:
                        tree.stop()
                    except Exception:
                        retain_lock = True
                        raise
                elif process and process.poll() is None:
                    # Only the direct Popen child is known if authentication failed.
                    process.kill()
                    process.wait(timeout=5)
        if process is not None and tree is None:
            retain_lock = True
            raise RuntimeError(
                "Complete native process ownership was not established; private log and lock retained"
            )
        # Never rewrite an output stream while any authenticated helper can hold it.
        redact(raw, state / "run.log")
        write_json(
            state / "processes.json",
            {
                "schema": 1,
                "processes": [asdict(value) for value in tree.members.values()] if tree else [],
                "allStopped": True,
                "cleanupSignals": tree.signals if tree else 0,
            },
        )
        if failure:
            raise failure
        if natural_exit != 0:
            raise RuntimeError("CodeOSS extension workflow exited nonzero")
        result = json.loads((state / "result.json").read_text())
        if (
            result.get("schema") != 1
            or result.get("nonce") != config["nonce"]
            or result.get("extensionId") != "elytra-owned.workbench-acceptance"
            or result.get("extensionFormat") != args.extension_format
        ):
            raise RuntimeError("No authenticated current extension-test result")
        if not tree or result.get("pid") not in tree.members or result["pid"] == process.pid:
            raise RuntimeError("Result did not originate from an observed owned extension host")
        if not all(
            result.get(name) is True
            for name in (
                "runtimeMite",
                "activated",
                "command",
                "opened",
                "editor",
                "edited",
                "saved",
            )
        ):
            raise RuntimeError("Actual extension/editor API steps did not complete")
        saved = Path(config["document"]).read_bytes()
        if saved != config["expected"].encode():
            raise RuntimeError("Independent native saved-file readback failed")
        write_json(
            state / "report.json",
            {
                "schema": 1,
                "accepted": True,
                "extensionFormat": args.extension_format,
                "nonce": config["nonce"],
                "savedSha256": hashlib.sha256(saved).hexdigest(),
                "nativeExit": natural_exit,
                "allOwnedProcessesStopped": True,
                "cleanupSignals": tree.signals,
                "stageManifestSha256": hashlib.sha256(
                    (stage / "ELYTRA-COMPILE.json").read_bytes()
                ).hexdigest(),
            },
        )
        print(
            "PASS actual CodeOSS extension activation, command, editor WorkspaceEdit/save and independent native readback"
        )
    finally:
        if not retain_lock:
            lock.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
