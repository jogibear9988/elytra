#!/usr/bin/env python3
"""Run real CodeOSS extension/editor acceptance offscreen or through a signed native bundle."""

import _support

import argparse
import ctypes
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "firefly/tests/code-oss-workbench"
OWNER = "elytra-code-oss-workbench-acceptance-v1"
MAX_LOG = 16 * 1024 * 1024
PROTOCOL_STAGES = (
    "client-admitted client-scope client-quota client-url client-sent client-revoke "
    "client-packet client-unknown client-response client-deadline dispatch-no-provider "
    "dispatch-scope dispatch-handler response-fail response-cancel response-frame "
    "response-cors response-script-mime server-packet server-quota server-admit "
    "server-authorize server-metadata route-scope route-missing route-response"
).split()
PROTOCOL_LINE = re.compile(
    rb"FIREFLY_PROTOCOL_DIAGNOSTIC stage=([a-z-]+) count=[0-9]{1,20} errno=-?[0-9]{1,10}"
)
CHANNEL_LINE = re.compile(
    rb"(?:[0-9]+\.[0-9]+ WebContent\([0-9]+\): )?"
    rb"FIREFLY_(NAVIGATION|DOCUMENT)_CHANNEL_READY pid=[0-9]+ epoch=[0-9]+"
)


IPC_ADMISSION_LINE = re.compile(
    rb"FIREFLY_IPC_ADMISSION_LIMIT reason=(queue|request-id|wire) "
    rb"document_waiting=[0-9]{1,20} renderer_waiting=[0-9]{1,20} packet_bytes=[0-9]{1,20}"
)


def startup_checkpoints(raw, startup_bytes):
    """Count allowlisted milestones from bytes written before process cleanup."""
    with raw.open("rb") as source:
        sample = source.read(min(startup_bytes, MAX_LOG))
    # A diagnostic line still being written at the cutoff is not evidence.
    checkpoints = {}
    for line in sample.split(b"\n")[:-1]:
        channel = CHANNEL_LINE.fullmatch(line)
        protocol = PROTOCOL_LINE.fullmatch(line)
        admission = IPC_ADMISSION_LINE.fullmatch(line)
        if channel:
            key = channel[1].decode("ascii").lower() + "-channel-ready"
        elif protocol and protocol[1].decode("ascii") in PROTOCOL_STAGES:
            key = protocol[1].decode("ascii")
        elif admission:
            key = "ipc-admission-" + admission[1].decode("ascii")
        else:
            continue
        checkpoints[key] = checkpoints.get(key, 0) + 1
    return {"checkpoints": checkpoints, "truncated": startup_bytes > len(sample)}


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
    def __init__(self, process, executable, bundle_runtime=None):
        self.process = process
        self.main_executables = {executable, bundle_runtime} if bundle_runtime else None
        self.bundle_runtime = bundle_runtime
        self.runtime_observed = False
        self.members = {}
        self.signals = 0
        current = native_process(process.pid)
        if (
            not current
            or current.parent != os.getpid()
            or current.uid != os.getuid()
            or current.session != process.pid
            or Path(current.executable).resolve() not in (self.main_executables or {executable})
        ):
            raise RuntimeError("Cannot authenticate the spawned native application")
        self.members[current.pid] = current
        self.runtime_observed = Path(current.executable).resolve() == bundle_runtime

    def scan(self):
        live = set()
        for pid, previous in list(self.members.items()):
            current = native_process(pid)
            if current and current.birth == previous.birth and current.uid == previous.uid:
                if self.main_executables and pid == self.process.pid:
                    executable = Path(current.executable).resolve()
                    if executable not in self.main_executables:
                        raise RuntimeError(
                            "Packaged main executed outside its verified bundle allowlist"
                        )
                    if executable == self.bundle_runtime:
                        self.runtime_observed = True
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


def prepare(
    state,
    stage,
    extension_format="esm",
    terminal=False,
    parent_liveness=False,
    workspace_switch=False,
    *,
    sealed_entry=False,
    native_title_bar=True,
):
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
    workspace = state / "workspace"
    edit_workspace = state / "opened-workspace" if workspace_switch else workspace
    # Match Code-OSS's transient CLI directories. Some of these default to
    # paths in the user's home even when --user-data-dir is supplied.
    storage = {
        "--user-data-dir": profile,
        "--extensions-dir": state / "extensions",
        "--shared-data-dir": state / "shared",
        "--agent-plugins-dir": state / "agent-plugins",
        "--agents-user-data-dir": state / "agents-data",
        "--agents-extensions-dir": state / "agents-extensions",
    }
    for directory in (*storage.values(), user, extension, workspace, edit_workspace, state / "xdg"):
        directory.mkdir(mode=0o700, exist_ok=True)
    settings = {
        "keyboard.touchbar.enabled": False,
        "workbench.startupEditor": "none",
        "security.workspace.trust.enabled": False,
    }
    if native_title_bar:
        settings["window.titleBarStyle"] = "native"
    write_json(user / "settings.json", settings)
    for name in ("extension.mjs", "extension.cjs", "commonjs-helper.cjs", "tests.mjs"):
        (extension / name).write_bytes((FIXTURE / name).read_bytes())
    manifest = json.loads((FIXTURE / "package.json").read_text())
    manifest["main"] = "./extension.cjs" if extension_format == "commonjs" else "./extension.mjs"
    if workspace_switch:
        manifest["activationEvents"] = ["onStartupFinished"]
    write_json(extension / "package.json", manifest)
    nonce = str(uuid.uuid4())
    document = edit_workspace / "owned-workbench.txt"
    initial = "owned-workbench-input\n"
    prefix = "edited-through-vscode-api:" + nonce + "\n"
    result = state / "result.json"
    for output in (
        result,
        state / "result.json.tmp",
        state / "report.json",
        state / "startup.json",
        state / "processes.json",
        state / "terminal-ready.json",
        state / "terminal-ready.json.tmp",
        state / "terminal-gate.json",
        state / "terminal-gate.json.tmp",
        state / "workspace-switch.json",
        edit_workspace / "owned-terminal.txt",
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
    if parent_liveness:
        config["parentLiveness"] = True
    if workspace_switch:
        config["workspaceSwitch"] = {
            "initialFolder": str(workspace),
            "targetFolder": str(edit_workspace),
            "record": str(state / "workspace-switch.json"),
        }
    if terminal:
        config.update(
            terminal=True,
            terminalWorkspace=str(edit_workspace),
            terminalOutput=str(edit_workspace / "owned-terminal.txt"),
            terminalText="terminal-through-vscode:" + nonce + "\n",
            terminalReady=str(state / "terminal-ready.json"),
            terminalGate=str(state / "terminal-gate.json"),
        )
    write_json(extension / "acceptance.json", config)
    entry = "main.mjs" if sealed_entry else "src/main.js"
    command = [str(stage / entry)]
    if not sealed_entry:
        command.append("--disable-crash-reporter")
    for option, directory in storage.items():
        command.extend([option, str(directory)])
    command.extend(["--extensionDevelopmentPath", str(extension)])
    # The upstream extension-test lifecycle kills the entire app when the
    # source extension host unloads. A normal activation must survive the switch.
    if not workspace_switch:
        command.extend(["--extensionTestsPath", str(extension / "tests.mjs")])
    command.extend(
        [
            "--new-window",
            "--skip-welcome",
            "--disable-workspace-trust",
            "--log",
            "trace",
            str(workspace),
        ]
    )
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
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--firefly", type=Path)
    selection.add_argument(
        "--bundle", type=Path, help="Launch this relocated, signed macOS Code-OSS.app"
    )
    parser.add_argument("--stage", type=Path)
    parser.add_argument(
        "--state-directory", type=Path, default=Path("/tmp/ef-workbench-acceptance")
    )
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument(
        "--default-title-bar",
        action="store_true",
        help="Use Code-OSS's normal title-bar setting in the Cocoa bundle",
    )
    parser.add_argument(
        "--workspace-switch",
        action="store_true",
        help="Use the public VS Code API to reopen a second folder in the same window before editing",
    )
    parser.add_argument(
        "--startup-diagnostics",
        action="store_true",
        help="Enable and report allowlisted native protocol milestones before process cleanup",
    )
    parser.add_argument(
        "--sample-entry",
        action="store_true",
        help="Verify and launch the native-package sample entry instead of original src/main.js",
    )
    parser.add_argument(
        "--terminal",
        action="store_true",
        help="Also verify an actual owned terminal shell workflow",
    )
    parser.add_argument(
        "--parent-liveness",
        action="store_true",
        help="Keep the real extension alive across parent-watch intervals and recheck its API",
    )
    parser.add_argument("--extension-format", choices=("esm", "commonjs"), default="esm")
    args = parser.parse_args()
    if os.name != "posix" or not (sys.platform == "darwin" or sys.platform.startswith("linux")):
        parser.error("Native descendant authentication is not implemented on this platform")
    if not 30 <= args.timeout <= 300:
        parser.error("--timeout must be 30..300 seconds")
    if args.default_title_bar and not args.bundle:
        parser.error("--default-title-bar requires the native Cocoa bundle")
    bundle_runtime = None
    if args.bundle:
        if sys.platform != "darwin" or args.stage or args.sample_entry:
            parser.error("--bundle requires macOS and derives its own stage/entry")
        bundle = args.bundle.resolve(strict=True)
        stage = bundle / "Contents/Resources/app"
        binary = bundle / "Contents/MacOS/Launcher"
        bundle_runtime = bundle / "Contents/MacOS/firefly"
        for executable in (binary, bundle_runtime):
            if (
                executable.resolve(strict=True) != executable
                or executable.is_symlink()
                or not executable.is_file()
                or not os.access(executable, os.X_OK)
            ):
                parser.error("Bundle executables must be physical executable files")
        if stage.resolve(strict=True) != stage:
            parser.error("Bundle app stage must be a physical directory within the verified bundle")
        subprocess.run(
            ["codesign", "--verify", "--deep", "--strict", str(bundle)],
            check=True,
            capture_output=True,
            timeout=30,
        )
        from build_code_oss import verify_sample

        verify_sample(stage)
    else:
        if not args.stage:
            parser.error("--firefly requires --stage")
        binary = args.firefly.resolve(strict=True)
        stage = args.stage.resolve(strict=True)
        if not (stage / "src/main.js").is_file() or not (stage / "ELYTRA-COMPILE.json").is_file():
            parser.error("Use the existing prepared CodeOSS stage")
        if args.sample_entry:
            from build_code_oss import verify_sample

            verify_sample(stage)
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
        arguments, config = prepare(
            state,
            stage,
            args.extension_format,
            args.terminal,
            args.parent_liveness,
            args.workspace_switch,
            sealed_entry=bool(args.bundle or args.sample_entry),
            native_title_bar=not args.default_title_bar,
        )
        if args.bundle:
            # Launcher owns --electron-compat and the sealed main.mjs prefix.
            # Supply only normal application CLI arguments, never another entry.
            command = [str(binary), *arguments[1:]]
        else:
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
            and key != "ELYTRA_CODE_OSS_LOCAL_DESKTOP"
        }
        environment.update(
            ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC="1",
            QT_QPA_PLATFORM="offscreen",
            XDG_CONFIG_HOME=str(state / "xdg"),
            XDG_DATA_HOME=str(state / "xdg"),
            XDG_CACHE_HOME=str(state / "xdg"),
        )
        if not (args.bundle or args.sample_entry):
            environment.update(VSCODE_DEV="1", ELYTRA_CODE_OSS_LOCAL_DESKTOP="1")
        if args.bundle:
            # This acceptance exercises the actual Cocoa provider; no offscreen
            # omission or inherited runtime diagnostic mode substitutes for it.
            environment = {
                key: value
                for key, value in environment.items()
                if not key.startswith(("FIREFLY_", "LIBWEB_FIREFLY_", "MITE_"))
            }
            environment.pop("ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC", None)
            environment["QT_QPA_PLATFORM"] = "cocoa"
        if args.startup_diagnostics:
            environment["FIREFLY_IPC_BYTES_DIAGNOSTIC"] = "1"
        raw = state / "run.private.log"
        raw.unlink(missing_ok=True)
        failure = None
        tree = None
        process = None
        natural_exit = None
        terminal_identity = None
        timed_out = False
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
                tree = OwnedTree(process, binary, bundle_runtime)
                end = time.monotonic() + args.timeout
                while process.poll() is None:
                    tree.scan()
                    if (
                        args.terminal
                        and terminal_identity is None
                        and Path(config["terminalReady"]).exists()
                    ):
                        ready_path = Path(config["terminalReady"])
                        if ready_path.stat().st_size > 4096:
                            raise RuntimeError("Invalid terminal ownership record")
                        ready = json.loads(ready_path.read_text())
                        pid = ready.get("pid")
                        if (
                            ready.get("schema") != 1
                            or ready.get("nonce") != config["nonce"]
                            or type(pid) is not int
                            or pid <= 0
                        ):
                            raise RuntimeError("Invalid terminal ownership identity")
                        current = native_process(pid)
                        observed = tree.members.get(pid)
                        if (
                            not current
                            or not observed
                            or current.birth != observed.birth
                            or current.uid != os.getuid()
                            or current.executable != observed.executable
                            or Path(current.executable).resolve() != Path("/bin/bash").resolve()
                        ):
                            raise RuntimeError(
                                "Terminal shell is not an authenticated owned native descendant"
                            )
                        terminal_identity = current
                        gate = Path(config["terminalGate"])
                        temporary_gate = gate.with_suffix(".json.tmp")
                        write_json(
                            temporary_gate, {"schema": 1, "nonce": config["nonce"], "pid": pid}
                        )
                        temporary_gate.replace(gate)
                    if raw.stat().st_size > MAX_LOG:
                        raise RuntimeError("Application diagnostic log exceeded 16 MiB")
                    if time.monotonic() >= end:
                        timed_out = True
                        raise RuntimeError("CodeOSS extension workflow timed out")
                    time.sleep(0.2)
                natural_exit = process.returncode
                tree.scan()
            except Exception as error:
                failure = error
            finally:
                startup_bytes = raw.stat().st_size if args.startup_diagnostics else 0
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
        startup = None
        if args.startup_diagnostics:
            startup = {
                "frontend": "cocoa" if args.bundle else "offscreen",
                "timedOut": timed_out,
                "nativeExit": natural_exit,
                "cleanupSignals": tree.signals if tree else 0,
                **startup_checkpoints(raw, startup_bytes),
            }
        redact(raw, state / "run.log")
        if startup is not None:
            write_json(state / "startup.json", startup)
            print("CODE_OSS_STARTUP " + json.dumps(startup), flush=True)
        write_json(
            state / "processes.json",
            {
                "schema": 1,
                "processes": [asdict(value) for value in tree.members.values()] if tree else [],
                "allStopped": True,
                "cleanupSignals": tree.signals if tree else 0,
                **(
                    {"bundleRuntimeObserved": tree.runtime_observed} if args.bundle and tree else {}
                ),
            },
        )
        if failure:
            raise failure
        if natural_exit != 0:
            raise RuntimeError("CodeOSS extension workflow exited nonzero")
        if args.bundle and not tree.runtime_observed:
            raise RuntimeError(
                "Packaged Launcher was never observed executing the exact bundled Firefly runtime"
            )
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
        if args.workspace_switch:
            transition = Path(config["workspaceSwitch"]["record"])
            if not transition.is_file() or transition.stat().st_size > 4096:
                raise RuntimeError("No owned workspace switch request")
            record = json.loads(transition.read_text())
            window_ids = re.findall(
                r"window#load: attempt to load window \(id: ([0-9]+)\)",
                (state / "run.log").read_text(errors="replace"),
            )
            if (
                record.get("nonce") != config["nonce"]
                or type(record.get("pid")) is not int
                or record["pid"] not in tree.members
                or record["pid"] in (process.pid, result["pid"])
                or result.get("workspaceSwitched") is not True
                or len(window_ids) != 2
                or window_ids[0] != window_ids[1]
            ):
                raise RuntimeError(
                    "No authenticated same-window folder switch and new extension host"
                )
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
        if args.parent_liveness and result.get("parentLiveness") is not True:
            raise RuntimeError("No authenticated extension parent-liveness continuation")
        saved = Path(config["document"]).read_bytes()
        if saved != config["expected"].encode():
            raise RuntimeError("Independent native saved-file readback failed")
        terminal_bytes = None
        if args.terminal:
            if (
                result.get("terminal") is not True
                or terminal_identity is None
                or result.get("terminalPid") != terminal_identity.pid
            ):
                raise RuntimeError("No authenticated terminal workflow result")
            terminal_bytes = Path(config["terminalOutput"]).read_bytes()
            if terminal_bytes != config["terminalText"].encode():
                raise RuntimeError("Independent native terminal file readback failed")
        write_json(
            state / "report.json",
            {
                "schema": 1,
                "accepted": True,
                "extensionFormat": args.extension_format,
                **({"workspaceSwitched": True} if args.workspace_switch else {}),
                **({"parentLiveness": True} if args.parent_liveness else {}),
                **(
                    {
                        "terminal": True,
                        "terminalProcess": asdict(terminal_identity),
                        "terminalSha256": hashlib.sha256(terminal_bytes).hexdigest(),
                    }
                    if args.terminal
                    else {}
                ),
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
        if args.workspace_switch:
            print("PASS actual CodeOSS same-window folder switch and new extension host")
        if args.parent_liveness:
            print("PASS actual CodeOSS parent liveness across three timed API continuations")
        if args.terminal:
            print(
                "PASS actual CodeOSS terminal shell ownership, sendText, exit17 and independent native readback"
            )
    finally:
        if not retain_lock:
            lock.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
