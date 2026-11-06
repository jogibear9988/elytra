/**
 * Declares the helper service lifecycle shared by product and diagnostic executables.
 * Initialization precedes configuration; shutdown releases native and realm ownership.
 */

#pragma once
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <LibWebCommon/WebDriver/BiDi.h>

namespace WebContent {
class PageClient;
}

namespace Firefly::RendererSession {
bool receive_ipc_reply(
    WebContent::PageClient&, u64, Web::HTML::CrossProcessId, String const&, JsonValue const&);

void initialize();

void apply(Web::WebDriver::BiDiSessionState);

void shutdown();
}
