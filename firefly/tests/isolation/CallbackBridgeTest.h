#pragma once
#include "worlds/IsolatedWorld.h"

namespace Web::DOM {
class Document;
}

ErrorOr<void> check_callback_bridge(JS::VM&, Web::DOM::Document&);
