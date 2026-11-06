/** Product-helper-only file script policy; no general CORS or filesystem bypass. */
#pragma once
#include <LibWeb/Export.h>

namespace Web::Fetch::Infrastructure {
class Request;
}

namespace Firefly::FileModules {
// Enable before application documents load; diagnostic/browser helpers stay disabled.
WEB_API void set_enabled(bool);

WEB_API bool allows(Web::Fetch::Infrastructure::Request const&);
}
