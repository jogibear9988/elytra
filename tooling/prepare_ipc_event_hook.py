#!/usr/bin/env python3
"""Prepare native IPC event/navigation hooks without changing upstream sources."""

import argparse
import hashlib
from pathlib import Path
from prepare_permission_hooks import adapt as permission_adapt

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "cd63de48c088f7d0127206ea464cd073728a6c645a117539c32326c49cf97930"
SEAM = "    if (auto* webdriver_bidi = WebDriverBiDi::the())\n        webdriver_bidi->did_receive_event(*this, navigable_id, name, move(params));"
HOOK = "    if (Firefly::receive_native_ipc(*this, navigable_id, name, params)) return;\n"

NAV_SEAM = "    async_create_navigation_params(ongoing_navigation->loader()->request());\n"
NAV_HOOK = """    NonnullRefPtr<WebContentPage> firefly_page { *this };
    auto firefly_navigable = navigable->make_weak_ptr();
    auto firefly_loader = ongoing_navigation->loader();
    Firefly::prepare_native_navigation(*this, firefly_loader->request());
    auto* firefly_current = firefly_navigable.ptr();
    if (!is_open() || !firefly_current
        || firefly_current->navigation_with_id(navigation_id) != ongoing_navigation
        || ongoing_navigation->worker() != this || ongoing_navigation->loader() != firefly_loader)
        return;
"""

DOC_SEAM = (
    "        host->async_populate_navigation(weak_loader->request(), weak_loader->take_result());\n"
)
DOC_HOOK = """        auto firefly_loader = weak_loader.strong_ref();
        auto firefly_navigable = navigable->make_weak_ptr();
        Firefly::prepare_native_document(*host, firefly_loader->request(), firefly_loader->result());
        auto* firefly_current = firefly_navigable.ptr();
        if (!host->is_open() || !firefly_current
            || firefly_current->navigation_with_id(navigation_id) != navigation
            || navigation->loader() != firefly_loader)
            return;
"""


def adapt(source):
    if source.count(SEAM) != 1 or source.count(NAV_SEAM) != 1 or source.count(DOC_SEAM) != 1:
        raise ValueError("Review WebContentPage IPC seam")
    return '#include "TransportHooks.h"\n' + source.replace(SEAM, HOOK + SEAM).replace(
        NAV_SEAM, NAV_HOOK + NAV_SEAM
    ).replace(DOC_SEAM, DOC_HOOK + DOC_SEAM)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    path = ROOT / "vendor/ladybird/Libraries/LibWebView/WebContentPage.cpp"
    if hashlib.sha256(path.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review owned native IPC event adapter before updating Ladybird")
    original = path.read_text()
    output = adapt(original)
    assert (
        output.removeprefix('#include "TransportHooks.h"\n')
        .replace(HOOK + SEAM, SEAM)
        .replace(NAV_HOOK + NAV_SEAM, NAV_SEAM)
        .replace(DOC_HOOK + DOC_SEAM, DOC_SEAM)
        == original
    )
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
