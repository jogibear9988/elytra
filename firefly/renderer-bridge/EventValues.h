#pragma once
#include <LibJS/Runtime/Completion.h>
#include <LibGC/Cell.h>
#include <LibWeb/Export.h>
namespace Web::DOM { class CustomEvent; }
namespace Firefly::EventValues {
enum class Activity { Unregistered, Live, Exiting, Revoked };
struct Hooks {
    Activity (*activity)(void*, JS::Realm&) { nullptr };
    void (*changed)(void*, Web::DOM::CustomEvent&) { nullptr };
    void (*visit)(void*, Web::DOM::CustomEvent&, GC::Cell::Visitor&) { nullptr };
    JS::ThrowCompletionOr<JS::Value> (*read)(void*, JS::Realm&, Web::DOM::CustomEvent&, JS::Value) { nullptr };
    void (*revoke)(void*, JS::Realm&) { nullptr };
};
WEB_API Activity activity(JS::Realm&);
WEB_API void changed(Web::DOM::CustomEvent&);
WEB_API void visit(Web::DOM::CustomEvent&, GC::Cell::Visitor&);
WEB_API JS::ThrowCompletionOr<JS::Value> read(JS::Realm&, Web::DOM::CustomEvent&, JS::Value);
WEB_API void revoke(JS::Realm&);
}
