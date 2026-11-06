/**
 * Defines native event-value snapshots that outlive source callbacks. Expose approved copied
 * data rather than retaining source event capabilities.
 */

#pragma once
#include <LibGC/Cell.h>
#include <LibJS/Runtime/Completion.h>
#include <LibWeb/Export.h>

namespace Web::DOM {
class Event;
}

namespace Firefly::EventValues {
enum class Activity { Unregistered, Live, Exiting, Revoked };

struct Hooks {
    Activity (*activity)(void*, JS::Realm&) { nullptr };
    void (*changed)(void*, Web::DOM::Event&, JS::Realm*) { nullptr };
    void (*visit)(void*, Web::DOM::Event&, GC::Cell::Visitor&) { nullptr };
    JS::ThrowCompletionOr<JS::Value> (*read)(
        void*, JS::Realm&, Web::DOM::Event&, JS::Value) { nullptr };
    void (*revoke)(void*, JS::Realm&) { nullptr };
};

WEB_API Activity activity(JS::Realm&);
// One payload slot per supported event subtype. JS binding entry points pass
// their writing realm; native construction passes nullptr, never ambient scope.
WEB_API void changed(Web::DOM::Event&, JS::Realm* writer);

WEB_API void visit(Web::DOM::Event&, GC::Cell::Visitor&);

WEB_API JS::ThrowCompletionOr<JS::Value> read(JS::Realm&, Web::DOM::Event&, JS::Value);

WEB_API void revoke(JS::Realm&);
}
