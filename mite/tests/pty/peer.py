#!/usr/bin/env python3
"""Owned real slave-terminal child; no GUI, audio, user terminal, or external network."""

import fcntl
import json
import os
from pathlib import Path
import signal
import struct
import sys
import termios

nonce, record = sys.argv[1:3]
winches = 0


def size():
    rows, cols, _, _ = struct.unpack("HHHH", fcntl.ioctl(0, termios.TIOCGWINSZ, bytes(8)))
    return {"rows": rows, "cols": cols}


def emit(kind, **values):
    # One terminal write prevents the line-discipline echo of the next input
    # from appearing between a response's JSON and its newline.
    data = (
        "@@PTY " + json.dumps({"kind": kind, "nonce": nonce, **values}, ensure_ascii=False) + "\n"
    ).encode()
    while data:
        data = data[os.write(1, data) :]


def resized(_signum, _frame):
    global winches
    winches += 1
    emit("winch", **size())


def interrupted(signum, _frame):
    emit("signal", signal=signum)
    raise SystemExit(42)


signal.signal(signal.SIGWINCH, resized)
signal.signal(signal.SIGINT, interrupted)
ctty = os.open("/dev/tty", os.O_RDWR)
try:
    info = {
        "pid": os.getpid(),
        "parent": os.getppid(),
        "group": os.getpgrp(),
        "session": os.getsid(0),
        "foreground": os.tcgetpgrp(ctty),
        "tty": [os.isatty(fd) for fd in (0, 1, 2)],
        "term": os.environ.get("TERM"),
        "owned": os.environ.get("ELYTRA_OWNED_PTY"),
        "cwd": os.getcwd(),
        **size(),
    }
finally:
    os.close(ctty)
with Path(record).open("a", encoding="utf-8") as output:
    output.write(json.dumps({"nonce": nonce, **info}) + "\n")
emit("ready", **info)
for line in sys.stdin:
    command = json.loads(line)
    if command["op"] == "echo":
        emit("echo", sequence=command["sequence"], text=command["text"])
    elif command["op"] == "size":
        emit("size", winches=winches, **size())
    elif command["op"] == "burst":
        for sequence in range(128):
            emit("burst", sequence=sequence, text="Grüße 🌍" + "x" * 4096)
    elif command["op"] == "exit":
        emit("last", code=command["code"])
        raise SystemExit(command["code"])
    else:
        raise RuntimeError("Unknown owned command")
