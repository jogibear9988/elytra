// Guard native listener entry before the upstream implementation wraps the
// receiver/Event. Keep the full original EventTarget algorithms and declarations.
#include "CallbackGate.h"
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/AbortSignal.h>
#include <LibWeb/WebSockets/WebSocket.h>
#include <LibWeb/XHR/XMLHttpRequest.h>
#include <LibWeb/Bindings/RustEventTarget.h>
namespace Web::Bindings {
JS::Completion firefly_original_invoke_event_listener(WebIDL::CallbackType&, DOM::Event&);
}
// Preserve the virtual notification; observe its successful add/remove sites.
#define event_listener_list_changed() (event_listener_list_changed(), Firefly::Callbacks::listener_changed(*this, listener))
// Preserve the original abort closure and capture its actual returned ID, even
// for a duplicate listener which upstream does not append to the target list.
#define add_abort_algorithm(...) add_abort_algorithm(__VA_ARGS__).map([&](auto id) { Firefly::Callbacks::listener_signal(*this, listener, id); return id; })
#define invoke_event_listener firefly_original_invoke_event_listener
#include <LibWeb/DOM/EventTarget.cpp>
#undef invoke_event_listener
#undef add_abort_algorithm
#undef event_listener_list_changed

namespace Firefly::Callbacks {
JS::Completion invoke_listener_original(Web::WebIDL::CallbackType& callback, Web::DOM::Event& event)
{
    return Web::Bindings::firefly_original_invoke_event_listener(callback, event);
}
}
namespace Web::Bindings {
JS::Completion invoke_event_listener(WebIDL::CallbackType& callback, DOM::Event& event)
{
    // exchange_gate is not used here: read the current VM gate through a small
    // exported dispatcher implemented alongside the existing callback registry.
    return Firefly::Callbacks::dispatch_listener(callback, event);
}
}
