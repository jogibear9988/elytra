#!/usr/bin/env python3
"""Compose owned report directory/application metadata without modifying Ladybird capture/storage."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '#include "native/crash/Reporter.h"\n'
FILES = {
    "CrashReportStore.cpp": (
        "0bcfdcb50dff9152ddb522427d1c8f51d3871d305734213052e36433377fde16",
        [
            (
                '    return LexicalPath::join(Core::StandardPaths::user_data_directory(), "Ladybird"sv, "CrashReports"sv).string();',
                "    if (auto directory = Firefly::CrashReporting::directory_override(); directory.has_value())\n        return *directory;\n",
            )
        ],
    ),
    "CrashReport.cpp": (
        "6c1770f8b0a4792bc083941e5fa21f3c738693c71013230f871905487ddc36d6",
        [
            (
                "    append_system_description(builder);\n    return builder.to_byte_string();",
                "    Firefly::CrashReporting::append_description(builder, process_type);\n",
            ),
            (
                "    m_saved_name = TRY(CrashReportStore { path }.store_report(m_process_type, builder.string_view(), crashed_time));",
                "    Firefly::CrashReporting::append_saved_description(builder, m_process_type);\n",
            ),
        ],
    ),
}


def adapt(source, seams):
    if any(source.count(seam) != 1 for seam, _ in seams):
        raise ValueError("Review native crash-report source seam")
    result = PREFIX + source
    for seam, hook in seams:
        # Description metadata belongs after build/system data; the other hooks precede their seam.
        if seam.startswith("    append_system_description"):
            result = result.replace(seam, seam.replace("    return", hook + "    return"))
        else:
            result = result.replace(seam, hook + seam)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, (digest, seams) in FILES.items():
        source = ROOT / "vendor/ladybird/Libraries/LibWebView" / name
        original = source.read_text()
        if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
            raise SystemExit("Review native crash adapters against the updated Ladybird pin")
        result = adapt(original, seams)
        restored = result.removeprefix(PREFIX)
        for _, hook in seams:
            restored = restored.replace(hook, "")
        assert restored == original
        for seam, _ in seams:
            for invalid in (original + seam, original.replace(seam, "")):
                try:
                    adapt(invalid, seams)
                except ValueError:
                    pass
                else:
                    raise AssertionError("Missing/duplicate crash seam accepted")
        output = args.output / name
        if not output.exists() or output.read_text() != result:
            output.write_text(result)
    print(
        "PASS native crash directory/metadata seams, fingerprints, exact restoration and drift refusal"
    )


if __name__ == "__main__":
    main()
