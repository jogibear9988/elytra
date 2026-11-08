/** Document-bound renderer services; native methods never cross contextBridge directly. */
#pragma once
#include <LibGC/Ptr.h>
#include <LibJS/Forward.h>

namespace Web::DOM {
class Document;
}

namespace Firefly {
class IsolatedWorld;
GC::Ref<JS::Object> create_web_frame(IsolatedWorld&, Web::DOM::Document&);
}
