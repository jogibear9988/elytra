#!/usr/bin/env python3
"""Compose native chooser ownership guards into the pinned Page implementation."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "vendor/ladybird/Libraries/LibWeb/Page/Page.cpp"
EXPECTED = "77907b9aafac290f454facbe83b300609015f658ff84191fcfd6f19e23671fc5"
INCLUDE = '#include "files/Requests.h"\n'
REQUEST = (
    "        m_client->page_did_request_file_picker(accepted_file_types, allow_multiple_files);"
)
REQUEST_HOOK = """        if (!m_pending_non_blocking_dialog_target || !Firefly::FileRequests::requested(*this, as<HTML::HTMLInputElement>(*m_pending_non_blocking_dialog_target), allow_multiple_files)) {
            file_picker_closed({});
            return;
        }
"""
CLOSED = "void Page::file_picker_closed(Span<HTML::SelectedFile> selected_files)\n{"
CLOSED_HOOK = "\n    Firefly::FileRequests::closed(*this);"
DELIVERY = "            input_element.did_select_files(selected_files);"
GUARDED = """            if (Firefly::FileRequests::allows(input_element, selected_files))
                input_element.did_select_files(selected_files);
            else
                input_element.did_select_files({});"""


def adapt(source):
    if (
        any(source.count(seam) != 1 for seam in [REQUEST, CLOSED, DELIVERY])
        or "Firefly::FileRequests" in source
    ):
        raise ValueError("Review native file request/page delivery seams")
    return INCLUDE + source.replace(REQUEST, REQUEST_HOOK + REQUEST).replace(
        CLOSED, CLOSED + CLOSED_HOOK
    ).replace(DELIVERY, GUARDED)


def prepare(output, input_source=SOURCE):
    original = input_source.read_bytes()
    if hashlib.sha256(original).hexdigest() != EXPECTED:
        raise ValueError("Review the owned native file request adapter against updated upstream")
    source = original.decode()
    generated = adapt(source)
    assert (
        generated.removeprefix(INCLUDE)
        .replace(REQUEST_HOOK + REQUEST, REQUEST)
        .replace(CLOSED + CLOSED_HOOK, CLOSED)
        .replace(GUARDED, DELIVERY)
        == source
    )
    for broken in [
        source.replace(REQUEST, ""),
        source.replace(CLOSED, ""),
        source.replace(DELIVERY, ""),
        source + REQUEST,
        generated,
    ]:
        try:
            adapt(broken)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/duplicate native picker seam accepted")
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text() != generated:
        output.write_text(generated)
    print(
        "PASS native chooser request/close/target guards, pin, exact upstream bytes and drift refusal"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.output)


if __name__ == "__main__":
    main()
