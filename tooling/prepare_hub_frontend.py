#!/usr/bin/env python3
"""Compose pinned Qt frontend services for Firefly's genuine in-process Hub.

Keep native view/input/backing-store dispatch intact. Browser stores, startup tabs
and implicit platform prompts are outside the embedded frontend; request/reply
providers receive genuine negative IPC completions instead of hanging or opening
unowned browser UI. The upstream checkout is never edited.
"""

import argparse
import hashlib
from pathlib import Path
import re

PINS = {
    "RemoteView.h": "7e360370cb1cb6ca0573568511f80fbce8e50934f91e15053aca66ee976aaaad",
    "RemoteView.cpp": "1c5842bc2fec803b076809982007638fcfc195a7cbc1962180ca161a12b924b0",
    "HubConnection.cpp": "7ca0485aaeedae09e630a2c3a16c1d94417dd86c5c5888877465ffbc4d0d2d17",
    "WebContentView.cpp": "8119597ba1a664a982cb9d0300c50d97568674cf133f3e757175a696a41f8430",
    "ContextMenus.cpp": "54fc415034b41e6a5000bcc7bae06342c5fafaad82e26b89b00e475847bb516b",
}


class Composer:
    def __init__(self, source):
        self.source = source
        self.changes = []

    def replace(self, before, after):
        if self.source.count(before) != 1:
            raise ValueError(f"Missing/duplicate frontend seam: {before[:100]}")
        offset = self.source.index(before)
        self.replace_at(offset, before, after)

    def replace_at(self, offset, before, after):
        if self.source[offset : offset + len(before)] != before:
            raise ValueError("Frontend source offset changed")
        self.changes.append((offset, before, after))
        self.source = self.source[:offset] + after + self.source[offset + len(before) :]

    def method(self, owner, name, body):
        pattern = rf"^([^\n]*\b{owner}::{name}\([^\n]*\)(?: const)?)\n\{{\n.*?^\}}"
        matches = list(re.finditer(pattern, self.source, re.M | re.S))
        if len(matches) != 1:
            raise ValueError(f"Missing/duplicate frontend method: {owner}::{name}")
        match = matches[0]
        signature = match[1]
        parameters = signature[signature.index("(") + 1 : signature.rindex(")")]
        # Commas inside template arguments/function types are not parameter separators.
        parts = []
        start = depth = 0
        for index, char in enumerate(parameters):
            if char in "<([":
                depth += 1
            elif char in ">)]":
                depth -= 1
            elif char == "," and depth == 0:
                parts.append(parameters[start:index])
                start = index + 1
        if parameters:
            parts.append(parameters[start:])
        unused = ""
        for parameter in parts:
            name_match = re.search(r"([A-Za-z_]\w*)\s*$", parameter)
            if not name_match:
                raise ValueError(f"Unnamed frontend parameter: {parameter}")
            unused += f"    (void){name_match[1]};\n"
        self.replace(match[0], signature + "\n{\n" + unused + body + "\n}")

    def verified(self, original):
        restored = self.source
        for offset, before, after in reversed(self.changes):
            if restored[offset : offset + len(after)] != after:
                raise ValueError("Frontend reverse composition failed")
            restored = restored[:offset] + before + restored[offset + len(after) :]
        if restored != original:
            raise ValueError("Frontend composition changed unowned bytes")
        return self.source


def transform(name, original):
    c = Composer(original)
    if name == "RemoteView.h":
        c.replace("class RemoteView : public BookmarkStoreObserver {", "class RemoteView {")
        c.replace(
            "    virtual void bookmarks_changed() override { update_bookmark_action(); }",
            "    void bookmarks_changed() { update_bookmark_action(); }",
        )
    else:
        c.replace("#include <UI/Qt/Application.h>", '#include "native/qt/hub/Frontend.h"')
    if name == "RemoteView.cpp":
        c.replace(
            "    : BookmarkStoreObserver(Application::bookmark_store())\n    , m_is_private(initial_state.is_private)",
            "    : m_is_private(initial_state.is_private)",
        )
        c.method(
            "RemoteView",
            "update_bookmark_action",
            "    m_toggle_bookmark_action->set_enabled(false);\n    m_toggle_bookmark_action->set_visible(false);",
        )
        for method in [
            "did_change_session_history",
            "did_visit_page",
            "did_change_visited_page_title",
            "did_finish_loading",
        ]:
            c.method(
                "RemoteView",
                method,
                "    // Browser persistence is not a Firefly capability. Canonical navigation history remains in the Hub.",
            )
        c.method(
            "RemoteView",
            "recent_history_entries",
            '    Firefly::HubFrontend::unsupported("browser-wide history"sv);\n    return {};',
        )
        c.method(
            "RemoteView",
            "did_change_favicon",
            "    // No browser favicon/history database is opened by the embedded frontend.\n    m_favicon_hash.clear();",
        )
        c.method(
            "RemoteView",
            "session_history_traversal_menu_items",
            """    Vector<SessionHistoryTraversalMenuItem> items;
    for (auto const& item : session_history_traversal_items(direction)) {
        auto url = item.url.serialize();
        items.append({ .step = item.step, .title = url, .url = move(url), .favicon_png = {} });
    }
    return items;""",
        )
        c.replace(
            "Application::the().show_bookmark_popover();",
            'Firefly::HubFrontend::unsupported("browser bookmark UI"sv);',
        )
        c.replace(
            "Application::the().activate_or_open_tab_with_url(url);",
            '(void)url;\n        Firefly::HubFrontend::unsupported("browser language settings tab"sv);',
        )
    if name == "HubConnection.cpp":
        # No browser frontend singleton, browser store or native authorization dialog.
        for include in [
            "ApplicationActions.h",
            "Bookmarks.h",
            "RemoteDownloads.h",
            "Tab.h",
            "WebUIPage.h",
        ]:
            c.replace(f"#include <UI/Qt/{include}>\n", "")
        fire_and_forget = [
            "app_open_url_in_new_tab",
            "app_open_url_in_new_window",
            "app_open_navigation_in_new_tab",
            "app_open_navigation_in_new_window",
            "app_update_tabs_display",
            "app_did_clear_history_since",
            "app_did_enable_devtools",
            "app_did_disable_devtools",
            "app_open_urls_in_new_tabs",
            "app_activate_or_open_tab_with_url",
            "app_display_download_confirmation_dialog",
            "download_added",
            "download_updated",
            "download_removed",
            "pip_hide",
            "pip_set_video_size",
            "pip_close",
            "app_focus_view_from_notification",
            "app_open_url_in_new_tab_from_notification",
            "external_url_release_handler",
            "app_open_new_tab",
            "app_open_new_window",
            "web_ui_opened",
            "web_ui_message",
            "web_ui_closed",
        ]
        for method in fire_and_forget:
            c.method(
                "HubConnection", method, f'    Firefly::HubFrontend::unsupported("{method}"sv);'
            )
        c.method(
            "HubConnection",
            "app_open_view_in_new_tab",
            '    Firefly::HubFrontend::unsupported("browser tab adoption"sv);\n    async_destroy_view(view_id);',
        )
        c.method(
            "HubConnection",
            "app_ask_user_for_download_path",
            '    Firefly::HubFrontend::unsupported("implicit browser download picker"sv);\n    async_answer_download_path(request_id, {});',
        )
        c.method(
            "HubConnection",
            "pip_open",
            '    Firefly::HubFrontend::unsupported("browser picture in picture"sv);\n    async_pip_did_close(view_id, true);',
        )
        c.method(
            "HubConnection",
            "notification_show",
            '    Firefly::HubFrontend::unsupported("implicit Web notification delivery"sv);\n    async_notification_failed(id);',
        )
        c.method("HubConnection", "notification_close", "    async_notification_closed(id, false);")
        c.method(
            "HubConnection",
            "microphone_request_authorization",
            "    async_microphone_authorization_changed(WebView::MicrophoneAuthorization::Denied);",
        )
        c.method(
            "HubConnection",
            "external_url_resolve_handler",
            '    Firefly::HubFrontend::unsupported("implicit external URL handler"sv);\n    async_external_url_handler_resolved(request_id, {}, String {}, false);',
        )
        c.method(
            "HubConnection",
            "external_url_launch",
            '    Firefly::HubFrontend::unsupported("implicit external URL launch"sv);\n    async_external_url_launched(request_id, false);',
        )
        c.method(
            "HubConnection",
            "app_read_clipboard_item",
            "    async_answer_clipboard_item(request_id, Firefly::HubFrontend::clipboard_item());",
        )
        c.method("HubConnection", "app_quit_for_signal", "    Core::EventLoop::current().quit(0);")
        c.replace(
            "#include <LibCore/Process.h>",
            "#include <LibCore/Process.h>\n#include <LibCore/EventLoop.h>",
        )
        c.replace(
            "        // NB: The application actions follow the active view only, and background tabs change state often.\n        if (auto* tab = Application::the().active_tab(); tab && tab->is_loaded() && &tab->view() == view)\n            ApplicationActions::the().update_for_active_view();",
            "        // Firefly owns application-menu state independently of the browser tab shell.",
        )
        # Spellchecker ownership remains the genuine Hub Application pointer,
        # including native false-only admission and the upstream null fallback.
    # Redirect only remaining explicit UI service references, never WebView::Application types.
    for before, after in [
        ("Application::the().translator()", "Firefly::HubFrontend::translator()"),
        (
            "Application::the().resolve_external_url_handler(",
            "Firefly::HubFrontend::resolve_external_url_handler(",
        ),
        ("Application::the().display_error_dialog(", "Firefly::HubFrontend::display_error_dialog("),
        ("Application::the().set_clipboard_text(", "Firefly::HubFrontend::set_clipboard_text("),
        (
            "Application::the().insert_clipboard_item(",
            "Firefly::HubFrontend::insert_clipboard_item(",
        ),
        ("Application::the().clipboard_text(", "Firefly::HubFrontend::clipboard_text("),
        ("Application::settings()", "Firefly::HubFrontend::settings()"),
        (
            "Application::supports_private_browsing_windows()",
            "Firefly::HubFrontend::supports_private_browsing_windows()",
        ),
        (
            "Application::the().supports_private_browsing_windows()",
            "Firefly::HubFrontend::supports_private_browsing_windows()",
        ),
        ("Application::the().spell_checker()", "WebView::Application::the().spell_checker()"),
    ]:
        # Repeated exact service calls are replaced separately at their actual offsets.
        pattern = re.compile(r"(?<!:)" + re.escape(before))
        while match := pattern.search(c.source):
            c.replace_at(match.start(), before, after)
    if re.search(r"(?<!:)\bApplication::", c.source):
        raise ValueError(f"Unowned Qt Application service remains in {name}")
    return c.verified(original)


def compose(name, data):
    if hashlib.sha256(data).hexdigest() != PINS[name]:
        raise ValueError(f"Pinned upstream frontend source changed: {name}")
    original = data.decode("utf-8")
    return transform(name, original).encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Pinned Ladybird checkout")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    outputs = {}
    for name in PINS:
        original = (args.source / "UI/Qt" / name).read_bytes()
        outputs[name] = compose(name, original)
        if args.self_test:
            for changed in [
                original + b"\n",
                original.replace(b"namespace Ladybird", b"namespace Drift", 1),
            ]:
                try:
                    compose(name, changed)
                except ValueError:
                    pass
                else:
                    raise ValueError("Frontend drift was accepted")
            try:
                compose(name, outputs[name])
            except ValueError:
                pass
            else:
                raise ValueError("Double frontend composition was accepted")
    if args.output:
        for name, data in outputs.items():
            target = args.output / "UI/Qt" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists() or target.read_bytes() != data:
                target.write_bytes(data)
    elif not args.self_test:
        parser.error("Require --output and/or --self-test")
    if args.self_test:
        print("PASS pinned Hub frontend composition byte reversal and drift rejection")


if __name__ == "__main__":
    main()
