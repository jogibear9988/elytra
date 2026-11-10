#!/usr/bin/env python3
"""Compose explicit UI callback retirement into the pinned actual Hub endpoint.

IPC deferred invocations retain the endpoint beyond HubHost's ownership. Detach
its application callbacks while the Hub is alive; late endpoint destruction must
not borrow the destroyed Application singleton. Ordinary current endpoints use
the same upstream cleanup, byte for byte. No class header or layout is changed.
"""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "Services/Hub/ConnectionFromUI.cpp"
DIGEST = "7b3a2aa378be1d22a3bad594831c2ae65a03275e1fe036113460ce300a742b5f"
PREFIX = '#include "native/qt/hub/ConnectionHooks.h"\n'
CLEANUP = """    WebView::ApplicationWebUI::set_callbacks({});
    Application::settings().on_persist = nullptr;
    auto& browser_process = Application::the().browser_process();
    browser_process.on_new_tab = nullptr;
    browser_process.on_new_window = nullptr;
    s_the = nullptr;
"""
DESTRUCTOR = "ConnectionFromUI::~ConnectionFromUI()\n{\n" + CLEANUP + "}\n"
DETACH = (
    """// A queued IPC invocation may retain this endpoint after the embedded Hub
// owner drops it. Release its real Application callbacks before that owner dies.
static void release_embedded_ui_callbacks(ConnectionFromUI& connection)
{
    if (s_the != &connection)
        return;
"""
    + CLEANUP
    + """}

ConnectionFromUI::~ConnectionFromUI()
{
    release_embedded_ui_callbacks(*this);
}
"""
)
DIE = """void ConnectionFromUI::die()
{
    // The UI is gone, which ends the browser, however that came to be.
    Core::EventLoop::current().quit(0);
}
"""
OWNED_DIE = DIE.replace("{\n", "{\n    if (s_the != this)\n        return;\n", 1)
SUFFIX = """
namespace Firefly::HubLifecycle {
void retire_ui_connection(Hub::ConnectionFromUI& connection)
{
    Hub::release_embedded_ui_callbacks(connection);
}
}
"""
SEAMS = ((DESTRUCTOR, DETACH), (DIE, OWNED_DIE))


def revert(source):
    if not source.startswith(PREFIX) or source.count(PREFIX) != 1:
        raise ValueError("Review Hub connection prefix")
    if not source.endswith(SUFFIX) or source.count(SUFFIX) != 1:
        raise ValueError("Review Hub connection retirement entry")
    result = source.removeprefix(PREFIX).removesuffix(SUFFIX)
    for before, after in SEAMS:
        if result.count(after) != 1:
            raise ValueError("Review Hub connection reversal seam")
        result = result.replace(after, before)
    return result


def adapt(original):
    if PREFIX in original or SUFFIX in original:
        raise ValueError("Repeated Hub connection composition")
    result = original
    for before, after in SEAMS:
        if result.count(before) != 1 or after in result:
            raise ValueError("Review Hub connection retirement seam")
        result = result.replace(before, after)
    result = PREFIX + result + SUFFIX
    if revert(result) != original:
        raise AssertionError("Hub connection changed bytes outside owned lifecycle seams")
    return result


def prepare(source, destination):
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != DIGEST:
        raise ValueError("Review Hub connection source pin")
    generated = adapt(raw.decode()).encode()
    if not destination.exists() or destination.read_bytes() != generated:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(generated)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    prepare(ROOT / "vendor/ladybird" / SOURCE, args.output)
    print("PASS pinned Hub endpoint callback retirement and byte preservation")


if __name__ == "__main__":
    main()
