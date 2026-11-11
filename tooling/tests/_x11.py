"""Independent X11 observations and a managed control client for isolated CI displays."""

from contextlib import contextmanager
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time

from test_renderer import stop


def query(*args, env=None):
    return subprocess.check_output(
        args,
        text=True,
        timeout=5,
        stderr=subprocess.PIPE,
        env={**(os.environ if env is None else env), "LC_ALL": "C"},
    )


def property_values(window, name, env=None):
    """Read server properties without consulting the owned Qt geometry adapter."""
    output = query("xprop", "-id", str(window), name, env=env)
    if " = " not in output:
        return []
    return [int(value, 0) for value in output.split(" = ", 1)[1].strip().split(", ")]


def root_property(name, env=None):
    output = query("xprop", "-root", name, env=env)
    if "window id #" in output:
        return output.split("window id #", 1)[1].strip()
    return output.split(" = ", 1)[1].strip() if " = " in output else ""


def wait_for(predicate, message):
    deadline = time.monotonic() + 10
    last = None
    while time.monotonic() < deadline:
        last = predicate()
        if last:
            return last
        time.sleep(0.03)
    raise AssertionError((message, last))


def isolated_display(env):
    """Refuse to start test windows on an ordinary developer desktop."""
    if (
        sys.platform != "linux"
        or not env.get("DISPLAY")
        or env.get("QT_QPA_PLATFORM") != "xcb"
        or env.get("FIREFLY_TEST_ISOLATED_X11") != "1"
    ):
        raise RuntimeError("Use a dedicated Linux Xvfb display with explicit isolated-X11 opt-in")


def process_state(process):
    """Preserve useful evidence when a live manager has stopped serving X requests."""
    state = {"pid": process.pid, "returncode": process.poll()}
    for name in ["status", "wchan"]:
        try:
            state[name] = (Path("/proc") / str(process.pid) / name).read_text()
        except OSError as error:
            state[name] = str(error)
    return state


@contextmanager
def managed_window_manager(env, log_path):
    """Require a real mapped/decorated non-Qt client before accepting any Qt windows.

    The EWMH manager property alone does not prove that Openbox is processing
    MapRequests. Keep this independent client alive until the server proves it
    is viewable and decorated, then remove it before exercising the application.
    """
    isolated_display(env)
    assert not root_property("_NET_SUPPORTING_WM_CHECK", env), "Use a fresh dedicated X server"
    log_path = Path(log_path)
    control = None
    observations = {}
    title = f"Firefly X11 WM control {os.getpid()}"
    with (
        tempfile.TemporaryDirectory(prefix="firefly-x11-wm-startup-") as startup_directory,
        log_path.open("wb") as log,
    ):
        startup_marker = Path(startup_directory) / "ready"
        # Openbox 3.6.1 openbox/openbox.c: screen_annex advertises the WM before
        # xqueue_listen/window_manage_all. run_startup_cmd runs only after those
        # and OB_STATE_RUNNING. An early MapRequest can otherwise get stranded
        # in its private queue while obt/xqueue.c's GLib source tests XPending.
        # Primary source: https://sources.debian.org/src/openbox/3.6.1-10/openbox/openbox.c/
        # This barrier only orders the control launch; mapping/decorations below
        # still independently prove the manager is actually serving clients.
        startup_command = shlex.join(
            [
                sys.executable,
                "-c",
                "import sys; open(sys.argv[1], 'xb').write(b'ready\\n')",
                str(startup_marker),
            ]
        )
        manager = subprocess.Popen(
            ["openbox", "--sm-disable", "--startup", startup_command],
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        try:
            wait_for(
                lambda: root_property("_NET_SUPPORTING_WM_CHECK", env), "Openbox advertisement"
            )

            def startup_completed():
                assert manager.poll() is None, ("Openbox exited during startup", manager.returncode)
                try:
                    return startup_marker.read_bytes() == b"ready\n"
                except FileNotFoundError:
                    return False

            wait_for(startup_completed, "Openbox post-initialization startup callback")
            control = subprocess.Popen(
                ["xmessage", "-title", title, "-buttons", "OK:0", "Window-manager readiness"],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )

            def managed_control():
                assert manager.poll() is None, ("Openbox exited", manager.returncode)
                assert control.poll() is None, ("X11 control exited", control.returncode)
                try:
                    info = query("xwininfo", "-name", title, env=env)
                except subprocess.CalledProcessError as error:
                    observations["lookup"] = error.stderr
                    return False
                observations["xwininfo"] = info
                match = re.search(r"Window id:\s*(0x[0-9a-fA-F]+)", info)
                assert match, info
                window = match[1]
                margins = property_values(window, "_NET_FRAME_EXTENTS", env)
                observations.update(window=window, margins=margins)
                return "Map State: IsViewable" in info and len(margins) == 4 and any(margins)

            wait_for(managed_control, "independent X11 control mapping and decorations")
            stop(control)
            control.wait(timeout=5)
            control = None
            print("PASS independent non-Qt X11 control is mapped and decorated")
            yield
            assert manager.poll() is None, "window manager exited during acceptance"
        finally:
            failed = sys.exc_info()[0] is not None
            if failed:
                observations["manager"] = process_state(manager)
                if control is not None:
                    observations["control"] = process_state(control)
                print(f"Independent X11 readiness observations: {observations}", file=sys.stderr)
            if control is not None:
                stop(control)
                control.wait(timeout=5)
            stop(manager)
            manager.wait(timeout=5)
            if failed:
                log.flush()
                print(log_path.read_text(errors="replace"), file=sys.stderr)
