#!/usr/bin/env python3
"""Add one authenticated synchronous permission query; preserve every upstream IPC ID."""

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
from generate_byte_bootstrap import GENERATOR, GENERATOR_SHA, message_ids, option, replace_option

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "vendor/ladybird/Services/WebContent/WebContentClient.ipc"
INPUT_SHA = "1f182d6324208db2f644d72b6fca3621de21b152f935b8a6fe104a6a2d875d13"
METHOD = "\n    [NotRouted] firefly_permission_check(u64 page, u64 document_ns, u64 document_local, u64 epoch, String name) => (bool granted)\n"
INCLUDE = "#include <LibIPC/FireflyBootstrap.h>\n"
DECLARATION = "    virtual Messages::WebContentClient::FireflyPermissionCheckResponse firefly_permission_check(u64 page, u64 document_ns, u64 document_local, u64 epoch, String name) = 0;"
BODY = """    virtual Messages::WebContentClient::FireflyPermissionCheckResponse firefly_permission_check(u64 page, u64 document_ns, u64 document_local, u64 epoch, String name)
    {
        return Firefly::Bytes::accept_permission_check(*this, page, document_ns, document_local, epoch, name);
    }"""


def extend(source):
    if (
        source.count("endpoint WebContentClient routed by Web::PageId page_id") != 1
        or not source.endswith("}\n")
        or "firefly_permission_check" in source
    ):
        raise ValueError("Review private permission endpoint seam")
    return INCLUDE + source[:-2] + METHOD + "}\n"


def main():
    arguments = sys.argv[1:]
    if (
        Path(option(arguments, "--input")).resolve() != INPUT.resolve()
        or option(arguments, "--header-name") != "WebContentClientEndpoint.h"
    ):
        raise ValueError("Permission generator selected for wrong endpoint")
    for source, expected in ((INPUT, INPUT_SHA), (GENERATOR, GENERATOR_SHA)):
        if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            raise ValueError("Review permission IPC after upstream update")
    output = Path(option(arguments, "--output"))
    implementation = Path(option(arguments, "--implementation"))
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="firefly-permission-query-", dir=output.parent
    ) as temporary:
        directory = Path(temporary)
        old_header, old_cpp = directory / "original.h", directory / "original.cpp"
        baseline = replace_option(
            replace_option(arguments, "--output", old_header), "--implementation", old_cpp
        )
        subprocess.run([sys.executable, str(GENERATOR), *baseline], check=True)
        source = INPUT.read_text()
        adapted = extend(source)
        assert adapted.removeprefix(INCLUDE).replace(METHOD, "") == source
        definition = directory / "WebContentClient.ipc"
        definition.write_text(adapted)
        subprocess.run(
            [sys.executable, str(GENERATOR), *replace_option(arguments, "--input", definition)],
            check=True,
        )
        header = output.read_text()
        before, after = message_ids(old_header.read_text()), message_ids(header)
        if (
            len(after) != len(before) + 2
            or any(after.get(name) != value for name, value in before.items())
            or after.get("FireflyPermissionCheck") != max(before.values()) + 1
            or after.get("FireflyPermissionCheckResponse") != max(before.values()) + 2
        ):
            raise ValueError("Permission query changed existing native IPC IDs")
        if header.count(DECLARATION) != 1 or not implementation.is_file():
            raise ValueError("Review permission query generated stub seam")
        output.write_text(header.replace(DECLARATION, BODY))
        for invalid in (source + METHOD, "", adapted):
            try:
                extend(invalid)
            except ValueError:
                pass
            else:
                raise AssertionError("Duplicate/ambiguous permission query accepted")
    print(f"PASS native permission query; preserved {len(before)} existing message IDs")


if __name__ == "__main__":
    main()
