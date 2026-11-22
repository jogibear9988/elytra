#!/usr/bin/env python3
"""Add the owned overlay lengths to LibWeb's existing per-document media snapshot."""

import argparse
import hashlib
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "vendor/ladybird/Libraries/LibWeb/CSS"
PINS = {
    "MediaList.cpp": "e129bdf8929a7d4acef0ead982733a4f608e06cb8580b626d7f26674495780d1",
    "RustMediaList.h": "cdac1ab997e587059621b598e69838d8ade31c917b06f233fabd8728e457b2de",
}


def compose(name, edits):
    data = (SOURCE / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != PINS[name]:
        raise ValueError(f"Review pinned titlebar CSS snapshot source: {name}")
    original = data.decode("utf-8")
    result = original
    for before, after in edits:
        if result.count(before) != 1:
            raise ValueError(
                f"Missing or ambiguous titlebar CSS snapshot seam in {name}: {before!r}"
            )
        result = result.replace(before, after, 1)
    restored = result
    for before, after in reversed(edits):
        if restored.count(after) != 1:
            raise ValueError(f"Ambiguous titlebar CSS snapshot reversal: {name}")
        restored = restored.replace(after, before, 1)
    if restored != original:
        raise ValueError(f"Titlebar CSS snapshot composition does not restore {name}")
    return result.encode("utf-8")


def prepare(output):
    source = compose(
        "MediaList.cpp",
        [
            (
                "#include <LibWeb/CSS/MediaList.h>",
                "#include <renderer-helper/title-bar/Overlay.h>\n#include <LibWeb/CSS/MediaList.h>",
            ),
            (
                "    for (size_t index = 0; index < m_values.size(); ++index)\n        m_values[index] = document.window()->query_media_feature(static_cast<MediaFeatureID>(index));",
                """    for (size_t index = 0; index < media_feature_count; ++index)
        m_values[index] = document.window()->query_media_feature(static_cast<MediaFeatureID>(index));

    if (auto titlebar = Firefly::TitleBarOverlay::css_geometry(document); titlebar.has_value()) {
        for (auto length : *titlebar) {
            m_values[m_value_count++] = {
                .kind = Parser::ValueParserFFI::FfiMediaFeatureValueKind::Length,
                .keyword = 0,
                .value = length,
                .second_value = 0,
            };
        }
    }""",
            ),
        ],
    )
    header = compose(
        "RustMediaList.h",
        [
            (".value_count = m_values.size(),", ".value_count = m_value_count,"),
            (
                "FfiMediaFeatureValue, media_feature_count> m_values {};",
                """FfiMediaFeatureValue, media_feature_count + 4> m_values {};
    size_t m_value_count { media_feature_count };""",
            ),
        ],
    )
    for path, data in {
        output / "MediaList.cpp": source,
        output / "include/LibWeb/CSS/RustMediaList.h": header,
    }.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    prepare(parser.parse_args().output)
