#pragma once
#include <LibWeb/WebIDL/AbstractOperations.h>

namespace Firefly::Callbacks {
using Invoke = JS::Completion (*)(void*, Web::WebIDL::CallbackType&, Optional<JS::Value>, Web::WebIDL::ExceptionBehavior, ReadonlySpan<JS::Value>);
struct Gate {
    void* context { nullptr };
    Invoke invoke { nullptr };
};
WEB_API Gate exchange_gate(JS::VM&, Gate);
WEB_API void restore_gate(JS::VM&, void* context, Gate previous);
WEB_API JS::Completion invoke_original(Web::WebIDL::CallbackType&, Optional<JS::Value>, Web::WebIDL::ExceptionBehavior, ReadonlySpan<JS::Value>);
}
