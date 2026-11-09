/** Native canvas defaults for authenticated top-level browsing contexts; never DOM styles. */
#pragma once
#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibGfx/Color.h>
#include <LibWeb/Export.h>

namespace Web::DOM {
class Document;
}

namespace Firefly::Canvas {
struct Configuration {
    String context;
    u32 generation;
    u32 color;
};

WEB_API ErrorOr<void> apply(Vector<Configuration>);

WEB_API void shutdown();

WEB_API Gfx::Color base(Web::DOM::Document const&, Gfx::Color fallback);
}
