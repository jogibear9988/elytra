#pragma once
#include "CallbackGate.h"

namespace Firefly::Handlers {
// Native document attribute algorithms use the principal map even when an
// isolated caller mutates the DOM. Overrides are only used for native cleanup.
class PrincipalScope {
public:
    explicit PrincipalScope(JS::VM&);
    ~PrincipalScope();
private:
    JS::VM* m_previous;
};
Callbacks::HandlerMap& select_map(Web::DOM::EventTarget&, Callbacks::HandlerMap& principal);
JS::Realm& listener_realm(Web::DOM::EventTarget&, JS::Realm& fallback);
void visit_maps(Web::DOM::EventTarget&, GC::Cell::Visitor&);
void clear_maps(Web::DOM::EventTarget&);
}
