#!/usr/bin/env python3
"""Append trusted endpoint handoffs using the unchanged pinned IPC generator."""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "vendor/ladybird/Services/WebContent/WebContentServer.ipc"
GENERATOR = ROOT / "vendor/ladybird/Meta/Generators/generate_ipc_definitions.py"
INPUT_SHA = "f07a0c368d61fb6f8b1761de77c437e6c56e0ef7184146aaf4676ef49e6784c3"
GENERATOR_SHA = "16c0220e4d722ee081ac1237cfe47f821ed7059cbd3f9c9c5914c6fac7b90c97"
INCLUDE = "#include <LibIPC/FireflyBootstrap.h>\n"
METHOD = "\n    [NotRouted] firefly_connect_byte_channel(IPC::TransportHandle handle, u64 high, u64 low) =|\n"
DECLARATION = "    virtual void firefly_connect_byte_channel(IPC::TransportHandle handle, u64 high, u64 low) = 0;"
BODY = """    virtual void firefly_connect_byte_channel(IPC::TransportHandle handle, u64 high, u64 low)
    {
        Firefly::Bytes::accept_bootstrap(move(handle), high, low);
    }"""

NAV_METHOD = "\n    [NotRouted] firefly_connect_navigation(IPC::TransportHandle handle, u64 page, u64 ns, u64 local, Utf16String navigation, String url, u64 epoch) =|\n"
NAV_DECLARATION = "    virtual void firefly_connect_navigation(IPC::TransportHandle handle, u64 page, u64 ns, u64 local, Utf16String navigation, String url, u64 epoch) = 0;"
NAV_BODY = """    virtual void firefly_connect_navigation(IPC::TransportHandle handle, u64 page, u64 ns, u64 local, Utf16String navigation, String url, u64 epoch)
    {
        Firefly::Bytes::accept_navigation(*this, move(handle), page, ns, local, move(navigation), move(url), epoch);
    }"""


def extend(source):
    if (
        source.count("endpoint WebContentServer routed by Web::PageId page_id") != 1
        or not source.endswith("}\n")
        or "firefly_connect_byte_channel" in source
        or "firefly_connect_navigation" in source
    ):
        raise ValueError("Review pinned control endpoint layout")
    result = INCLUDE + source[:-2] + METHOD + NAV_METHOD + "}\n"
    assert result.removeprefix(INCLUDE).replace(METHOD, "").replace(NAV_METHOD, "") == source
    return result


def message_ids(header):
    matches = re.findall(r"enum class MessageID : i32 \{\n(.*?)\n\};", header, re.S)
    if len(matches) != 1:
        raise ValueError("Review generated message ID declaration")
    return {
        name: int(number) for name, number in re.findall(r"^\s*(\w+) = (\d+),$", matches[0], re.M)
    }


def option(arguments, name):
    if arguments.count(name) != 1:
        raise ValueError(f"Expected one {name}")
    return arguments[arguments.index(name) + 1]


def replace_option(arguments, name, value):
    result = list(arguments)
    result[result.index(name) + 1] = str(value)
    return result


def main():
    arguments = sys.argv[1:]
    if (
        Path(option(arguments, "--input")).resolve() != INPUT.resolve()
        or option(arguments, "--header-name") != "WebContentServerEndpoint.h"
    ):
        raise ValueError("Owned bootstrap generator selected for wrong endpoint")
    for path, expected in ((INPUT, INPUT_SHA), (GENERATOR, GENERATOR_SHA)):
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError("Review byte control generator against the updated Ladybird pin")
    output = Path(option(arguments, "--output"))
    implementation = Path(option(arguments, "--implementation"))
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="firefly-byte-control-", dir=output.parent
    ) as directory:
        directory = Path(directory)
        original_header, original_cpp = directory / "original.h", directory / "original.cpp"
        baseline = replace_option(
            replace_option(arguments, "--output", original_header), "--implementation", original_cpp
        )
        subprocess.run([sys.executable, str(GENERATOR), *baseline], check=True)
        definition = directory / "WebContentServer.ipc"
        definition.write_text(extend(INPUT.read_text()))
        subprocess.run(
            [sys.executable, str(GENERATOR), *replace_option(arguments, "--input", definition)],
            check=True,
        )
        header = output.read_text()
        before, after = message_ids(original_header.read_text()), message_ids(header)
        if (
            not before
            or len(after) != len(before) + 2
            or any(after.get(name) != number for name, number in before.items())
            or after.get("FireflyConnectByteChannel") != max(before.values()) + 1
            or after.get("FireflyConnectNavigation") != max(before.values()) + 2
        ):
            raise ValueError("Existing control message IDs changed")
        if (
            header.count(DECLARATION) != 1
            or header.count(NAV_DECLARATION) != 1
            or not implementation.is_file()
        ):
            raise ValueError("Review owned control stub seam")
        header = header.replace(DECLARATION, BODY).replace(NAV_DECLARATION, NAV_BODY)
        output.write_text(header)
        # A duplicate declaration must fail rather than gain another control route.
        try:
            extend(definition.read_text())
        except ValueError:
            pass
        else:
            raise AssertionError("Duplicate native control method accepted")
    print(f"PASS native byte control generation; preserved {len(before)} upstream message IDs")


if __name__ == "__main__":
    main()
