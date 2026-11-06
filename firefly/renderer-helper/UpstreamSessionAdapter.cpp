/**
 * Composes the pinned renderer session-state setter. Keep upstream code intact while routing
 * the owned preload/diagnostic service through one ABI seam.
 */

// This direct object supplies the same service functions as the archive's BiDi
// object, so that archive member is not extracted. Keep its implementation intact
// and give only the session-state setter a private name for our composition seam.
#include "RendererSession.h"
#include <WebContent/WebDriverBiDi.h>
// Preload service declarations before the token adapter: class/IPC method names
// must stay identical to those in the separately compiled service archive.
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageClient.h>

namespace WebContent {
void firefly_upstream_set_session_state(Web::WebDriver::BiDiSessionState);
}

#define set_webdriver_bidi_session_state firefly_upstream_set_session_state
#include <Services/WebContent/WebDriverBiDi.cpp>
#undef set_webdriver_bidi_session_state

namespace WebContent {
void set_webdriver_bidi_session_state(Web::WebDriver::BiDiSessionState state)
{
    Firefly::RendererSession::apply(move(state));
}
}
