#!/usr/bin/env python3
"""Prepare native IPC event/navigation hooks without changing upstream sources."""

import argparse
import hashlib
from pathlib import Path
from prepare_permission_hooks import adapt as permission_adapt

# Reviewed 9803f2d5: only native notification-test handlers were added here;
# navigation handoff, native-document preparation and IPC event ordering are intact.
ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "5c4e01169ba9e1cce382eab131346797edc5782a601749341b82ce213ff0e769"
SEAM = "    if (auto* webdriver_bidi = WebDriverBiDi::the())\n        webdriver_bidi->did_receive_event(*this, navigable_id, name, move(params));"
HOOK = "    if (Firefly::receive_native_ipc(*this, navigable_id, name, params)) return;\n"

NAV_SEAM = "    async_create_navigation_params(ongoing_navigation->loader()->request());\n"
NAV_HOOK = """    NonnullRefPtr<WebContentPage> firefly_page { *this };
    auto firefly_navigable = navigable->make_weak_ptr();
    auto firefly_loader = ongoing_navigation->loader();
    Firefly::diagnose_navigation_phase("before-native-navigation");
    Firefly::prepare_native_navigation(*this, firefly_loader->request());
    Firefly::diagnose_navigation_phase("after-native-navigation");
    auto* firefly_current = firefly_navigable.ptr();
    if (!is_open() || !firefly_current
        || firefly_current->navigation_with_id(navigation_id) != ongoing_navigation
        || ongoing_navigation->worker() != this || ongoing_navigation->loader() != firefly_loader)
        return;
"""

DOC_SEAM = "            host->async_populate_navigation(weak_loader->request(), weak_loader->take_result());\n"
DOC_HOOK = """            auto firefly_loader = weak_loader.strong_ref();
            auto firefly_navigable = navigable->make_weak_ptr();
            Firefly::diagnose_navigation_phase("before-native-document");
            Firefly::prepare_native_document(*host, firefly_loader->request(), firefly_loader->result());
            Firefly::diagnose_navigation_phase("after-native-document");
            auto* firefly_current = firefly_navigable.ptr();
            if (!host->is_open() || !firefly_current
                || firefly_current->navigation_with_id(navigation_id) != navigation
                || navigation->loader() != firefly_loader)
                return;
"""


# Scalar observations only. The root-owned hook gates these at runtime, default
# off, and bounds their count. No native identity, URL, body or error text enters
# these calls. Each original statement is retained byte-for-byte.
PHASES = (
    (
        "bool WebContentPage::continue_navigation_population_in_selected_process(Web::HTML::CrossProcessId navigable_id, Utf16String navigation_id)\n{\n",
        'bool WebContentPage::continue_navigation_population_in_selected_process(Web::HTML::CrossProcessId navigable_id, Utf16String navigation_id)\n{\n    Firefly::diagnose_navigation_phase("population-entry");\n',
    ),
    (
        "    auto finish_handoff = [host, navigable_id, navigation_id, weak_loader](bool succeeded) {\n",
        '    auto finish_handoff = [host, navigable_id, navigation_id, weak_loader](bool succeeded) {\n        Firefly::diagnose_navigation_phase("handoff-callback", succeeded ? 1 : 0);\n',
    ),
    (
        "        weak_loader->let_host_adopt_response_body(*host, [host, navigable_id, navigation_id, weak_loader] {\n",
        '        Firefly::diagnose_navigation_phase("adopt-request");\n        weak_loader->let_host_adopt_response_body(*host, [host, navigable_id, navigation_id, weak_loader] {\n            Firefly::diagnose_navigation_phase("adopt-callback");\n',
    ),
    (
        DOC_SEAM + "        });\n    };",
        '            Firefly::diagnose_navigation_phase("before-populate");\n'
        + DOC_SEAM
        + '            Firefly::diagnose_navigation_phase("after-populate");\n        });\n        Firefly::diagnose_navigation_phase("adopt-submitted");\n    };',
    ),
    (
        "    weak_loader->move_response_body_to(host->client().request_server_site_bindings(), move(finish_handoff));\n",
        '    Firefly::diagnose_navigation_phase("handoff-request");\n    weak_loader->move_response_body_to(host->client().request_server_site_bindings(), move(finish_handoff));\n    Firefly::diagnose_navigation_phase("handoff-submitted");\n',
    ),
)
PREFIX = '#include "TransportHooks.h"\n'


def revert(source):
    if not source.startswith(PREFIX) or source.count(PREFIX) != 1:
        raise ValueError("Missing/duplicate native IPC hook prefix")
    source = source.removeprefix(PREFIX)
    for original, replacement in reversed(PHASES):
        if source.count(replacement) != 1:
            raise ValueError("Missing/duplicate native navigation phase")
        source = source.replace(replacement, original)
    for original, hook in ((SEAM, HOOK), (NAV_SEAM, NAV_HOOK), (DOC_SEAM, DOC_HOOK)):
        if source.count(hook + original) != 1:
            raise ValueError("Missing/duplicate native IPC hook")
        source = source.replace(hook + original, original)
    if hashlib.sha256(source.encode()).hexdigest() != EXPECTED:
        raise ValueError("Native IPC hook original fingerprint differs")
    return source


def adapt(source):
    if hashlib.sha256(source.encode()).hexdigest() != EXPECTED:
        raise ValueError("Review WebContentPage native IPC source fingerprint")
    if source.count(SEAM) != 1 or source.count(NAV_SEAM) != 1 or source.count(DOC_SEAM) != 1:
        raise ValueError("Review WebContentPage IPC seam")
    output = PREFIX + source.replace(SEAM, HOOK + SEAM).replace(
        NAV_SEAM, NAV_HOOK + NAV_SEAM
    ).replace(DOC_SEAM, DOC_HOOK + DOC_SEAM)
    for original, replacement in PHASES:
        if output.count(original) != 1:
            raise ValueError("Review WebContentPage navigation diagnostic seam")
        output = output.replace(original, replacement)
    if revert(output) != source:
        raise ValueError("Native IPC hook reversal differs")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    path = ROOT / "vendor/ladybird/Libraries/LibWebView/WebContentPage.cpp"
    if hashlib.sha256(path.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review owned native IPC event adapter before updating Ladybird")
    original = path.read_text()
    output = adapt(original)
    assert revert(output) == original
    for invalid in [
        "",
        original + SEAM,
        original.replace(SEAM, ""),
        original + NAV_SEAM,
        original.replace(NAV_SEAM, ""),
        original + DOC_SEAM,
        original.replace(DOC_SEAM, ""),
    ]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Ambiguous IPC seam accepted")
    # Compose both owned boundaries from the exact same pinned translation unit.
    # Permission gates must not discard the existing IPC/navigation hooks.
    output = permission_adapt("WebContentPage.cpp", output)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print("PASS owned native IPC event seam, upstream fingerprint and byte preservation")


if __name__ == "__main__":
    main()
