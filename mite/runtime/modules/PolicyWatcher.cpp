/** Realm-local extension module; standalone Mite owns policy services without Firefly/LibWeb. */
#include "BuiltinSources.h"
#include "core/EventLoop.h"
#include "modules/ModuleLoader.h"
#include "platform/policy/Host.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/Script.h>

namespace Mite {
JS::ThrowCompletionOr<void> ModuleLoader::install_policy_watcher()
{
    auto parsed = JS::Script::parse(Utf16String::from_utf8(StringView(MITE_POLICY_WATCHER_SOURCE,
                                        sizeof(MITE_POLICY_WATCHER_SOURCE) - 1)),
        m_realm, "mite:policy-watcher/factory"sv);
    VERIFY(!parsed.is_error());
    GC::Root script { parsed.release_value() };
    auto value = TRY(m_vm.run(*script));
    VERIFY(value.is_function());
    GC::Root factory { value.as_function() };
    GC::Root native { Policy::create_host(m_realm, m_event_loop) };
    auto exports = TRY(JS::call(m_vm, *factory, JS::js_undefined(), native.ptr()));
    VERIFY(exports.is_object());
    return register_host_module("mite-policy-watcher", exports.as_object(), { "createWatcher" });
}
}
