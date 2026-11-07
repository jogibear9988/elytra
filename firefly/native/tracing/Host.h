/** Trusted main binding only; renderer/preload never receives recording controls. */
#pragma once
#include "Recorder.h"

namespace JS {
class Realm;
class Object;
}

namespace Firefly::Tracing {
void install_host(JS::Realm&, JS::Object&, Recorder&);
}
