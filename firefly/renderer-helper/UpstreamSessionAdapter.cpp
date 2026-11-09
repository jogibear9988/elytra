/**
 * Composes the pinned renderer session-state setter. Keep upstream code intact while routing
 * the owned preload/diagnostic service through one ABI seam.
 */

// Supply the same service symbols as the archive's BiDi object, so that member
// is not extracted. Rename only the session-state and command entry definitions;
// class/IPC declarations below retain their original ABI.
#include "RendererSession.h"
#include "canvas/Session.h"
#include <WebContent/WebDriverBiDi.h>
#include <cstdio>
#include <cstdlib>
// Preload service declarations before the token adapter: class/IPC method names
// must stay identical to those in the separately compiled service archive.
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageClient.h>

namespace WebContent {
void firefly_upstream_set_session_state(Web::WebDriver::BiDiSessionState);
void firefly_upstream_bidi_command(
    PageClient&, u64, Web::HTML::CrossProcessId, String const&, JsonValue);
}

#define run_webdriver_bidi_command firefly_upstream_bidi_command
#define set_webdriver_bidi_session_state firefly_upstream_set_session_state
#include <Services/WebContent/WebDriverBiDi.cpp>
#undef set_webdriver_bidi_session_state
#undef run_webdriver_bidi_command

namespace WebContent {
void run_webdriver_bidi_command(PageClient& page, u64 command, Web::HTML::CrossProcessId navigable,
    String const& name, JsonValue params)
{
    if (Firefly::RendererSession::receive_ipc_reply(page, command, navigable, name, params))
        return;
    firefly_upstream_bidi_command(page, command, navigable, name, move(params));
}

void set_webdriver_bidi_session_state(Web::WebDriver::BiDiSessionState state)
{
    auto canvas = Firefly::HelperCanvas::extract(state);
    if (canvas.is_error()) {
        warnln("Invalid native canvas configuration: {}", canvas.error());
        fflush(nullptr);
        std::_Exit(1);
    }

    Firefly::RendererSession::apply(move(state));
}
}
