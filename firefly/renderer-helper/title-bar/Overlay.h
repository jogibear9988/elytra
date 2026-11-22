/** Renderer-side geometry for the owned top-level window controls overlay. */
#pragma once

#include <AK/Error.h>
#include <LibWeb/Export.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebDriver/BiDi/BiDi.h>

namespace Firefly::TitleBarOverlay {
WEB_API ErrorOr<void> extract(Web::WebDriver::BiDiSessionState&);

WEB_API void install(Web::DOM::Document&);

WEB_API void shutdown();
}
