/** Realm-local PTY extension; descriptors and native callbacks remain private. */
#include "BuiltinSources.h"
#include "core/EventLoop.h"
#include "modules/ModuleLoader.h"
#include "platform/terminal/pty/Host.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/Script.h>

namespace Mite {
JS::ThrowCompletionOr<void> ModuleLoader::install_pty()
{
    GC::Root host { JS::Object::create(m_realm, nullptr) };
    host->define_direct_property(
        "native"_utf16, create_pty_host(m_realm, m_event_loop), JS::default_attributes);
    host->define_direct_property(
        "EventEmitter"_utf16, TRY(builtin("events")), JS::default_attributes);
    GC::Root buffer { TRY(builtin("buffer")) };
    host->define_direct_property(
        "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
    GC::Root decoder { TRY(builtin("string_decoder")) };
    host->define_direct_property(
        "StringDecoder"_utf16, TRY(decoder->get("StringDecoder"_utf16)), JS::default_attributes);
    GC::Root os { TRY(builtin("os")) };
    GC::Root constants { TRY(os->get("constants"_utf16)).as_object() };
    host->define_direct_property(
        "signals"_utf16, TRY(constants->get("signals"_utf16)), JS::default_attributes);

    auto parsed = JS::Script::parse(
        Utf16String::from_utf8(StringView(MITE_PTY_SOURCE, sizeof(MITE_PTY_SOURCE) - 1)), m_realm,
        "mite:pty/factory"sv);
    VERIFY(!parsed.is_error());
    GC::Root script { parsed.release_value() };
    auto value = TRY(m_vm.run(*script));
    VERIFY(value.is_function());
    GC::Root factory { value.as_function() };
    GC::Root exports { TRY(JS::call(m_vm, *factory, JS::js_undefined(), host.ptr())).as_object() };
    return register_host_module(
        "mite-pty", *exports, { "spawn", "fork", "createTerminal", "open" });
}
}
