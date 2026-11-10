#!/usr/bin/env python3
"""Compose owned report directory/application metadata without modifying Ladybird capture/storage."""

import argparse
import hashlib
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '#include "native/crash/Reporter.h"\n'
# Candidate storage adds cross-filesystem adoption, website sidecars and bounded
# pending retention. These hooks leave its ownership/no-follow/atomic-write paths
# intact: only the default directory and owned build metadata are composed.
FILES = {
    "CrashReportStore.cpp": (
        "b88fef186592ff8f8c80f44820ef8ee5c8b7a23840812be90d23f6e073031888",
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

APPLICATION_SOURCE = "Libraries/LibWebView/Application.cpp"
APPLICATION_DIGEST = "ad7c79c0e4c256acc45b5cfadd993545f84efe031ca30e30381d4cc63ae20d33"


def application_changes(source):
    """Keep browser report migration/disposal out of a trusted embedded profile."""
    start = "    // Reports belong to the profile they came from, whose own setting decides what happens to them. A temporary\n"
    end = "    // Test results must not depend on the languages of the machine running the tests.\n"
    if source.count(start) != 1 or source.count(end) != 1:
        raise ValueError("Review browser crash profile ownership seam")
    block = source[source.index(start) : source.index(end)]
    discard = (
        "    if (m_settings->crash_report_sending() == CrashReportSending::NeverSend)\n"
        "        CrashReportOutbox::the().discard_all();\n"
    )
    coordination = "    bool coordinate_browser_process = !headless_mode.has_value() && !webdriver_browser_endpoint.has_value() && !m_stats_log && should_coordinate_browser_process();"
    return [
        (
            coordination,
            coordination.replace(
                "= ", "= !Firefly::CrashReporting::owns_embedded_profile() && ", 1
            ),
        ),
        (
            block,
            "    if (!Firefly::CrashReporting::owns_embedded_profile()) {\n"
            + textwrap.indent(block.rstrip(), "    ")
            + "\n    }\n\n",
        ),
        (
            discard,
            "    if (!Firefly::CrashReporting::owns_embedded_profile()) {\n"
            + textwrap.indent(discard.rstrip(), "    ")
            + "\n    }\n",
        ),
    ]


def adapt_application(source):
    changes = application_changes(source)
    result = source
    for original, replacement in changes:
        if result.count(original) != 1:
            raise ValueError("Review missing/duplicate crash profile ownership seam")
        result = result.replace(original, replacement)
    restored = result
    for original, replacement in reversed(changes):
        restored = restored.replace(replacement, original)
    if restored != source:
        raise ValueError("Crash profile composition altered unrelated upstream bytes")
    return PREFIX + result


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
    source = ROOT / "vendor/ladybird" / APPLICATION_SOURCE
    original = source.read_text()
    if hashlib.sha256(source.read_bytes()).hexdigest() != APPLICATION_DIGEST:
        raise SystemExit("Review browser crash profile ownership after Ladybird update")
    result = adapt_application(original)
    for seam, _ in application_changes(original):
        for invalid in (original + seam, original.replace(seam, "")):
            try:
                adapt_application(invalid)
            except ValueError:
                pass
            else:
                raise AssertionError("Missing/duplicate crash profile seam accepted")
    output = args.output / "Application.cpp"
    if not output.exists() or output.read_text() != result:
        output.write_text(result)
    print(
        "PASS native crash directory/metadata seams, fingerprints, exact restoration and drift refusal"
    )


if __name__ == "__main__":
    main()
