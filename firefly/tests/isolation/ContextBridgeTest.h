#pragma once
#include "worlds/IsolatedWorld.h"

namespace Web::DOM {
class Document;
}

ErrorOr<void> check_context_bridge(JS::VM&, Web::DOM::Document&);

ErrorOr<void> check_large_context_bridge(JS::VM&, Firefly::IsolatedWorld&, Web::DOM::Document&);
