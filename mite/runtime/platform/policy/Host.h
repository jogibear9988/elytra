/** VM-owner boundary for policy snapshots. Native worker callbacks never enter JavaScript. */
#pragma once
#include "Provider.h"
#include <AK/Function.h>
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;

namespace Policy {
    using ProviderFactory
        = Function<ErrorOr<NonnullOwnPtr<Provider>>(ByteString, Vector<Definition>)>;
    GC::Ref<JS::Object> create_host(JS::Realm&, EventLoop&, ProviderFactory = {});
}
}
