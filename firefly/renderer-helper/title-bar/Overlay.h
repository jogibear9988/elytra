/** Renderer-side geometry for the owned top-level window controls overlay. */
#pragma once

#include <AK/Array.h>
#include <AK/Error.h>
#include <AK/Optional.h>
#include <LibWeb/Export.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebDriver/BiDi/BiDi.h>

namespace Firefly::TitleBarOverlay {
WEB_API ErrorOr<void> extract(Web::WebDriver::BiDiSessionState&);

WEB_API void install(Web::DOM::Document&);

WEB_API void shutdown();

// CSS-pixel x, y, width and height, in the order of the named CSS environment lengths.
Optional<Array<double, 4>> css_geometry(Web::DOM::Document const&);
}
