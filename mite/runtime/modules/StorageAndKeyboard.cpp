/** Explicit native Mite extensions; no Node/N-API or desktop renderer dependency. */
#include "BuiltinSources.h"
#include "core/EventLoop.h"
#include "modules/ModuleLoader.h"
#include "platform/filesystem/Clone.h"
#include "platform/keyboard/Host.h"
#include "storage/sqlite/Host.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/Script.h>

namespace Mite {
JS::ThrowCompletionOr<void> ModuleLoader::install_storage_and_keyboard()
{
    auto factory = [&](StringView source, JS::Object& host,
                       StringView name) -> JS::ThrowCompletionOr<GC::Ref<JS::Object>> {
        auto parsed = JS::Script::parse(Utf16String::from_utf8(source), m_realm, name);
        VERIFY(!parsed.is_error());
        GC::Root script { parsed.release_value() };
        auto value = TRY(m_vm.run(*script));
        VERIFY(value.is_function());
        GC::Root callable { value.as_function() };
        auto result = TRY(JS::call(m_vm, *callable, JS::js_undefined(), &host));
        VERIFY(result.is_object());
        return result.as_object();
    };

    GC::Root keyboard { Keyboard::create_host(m_realm, m_event_loop) };
    GC::Root keymap { TRY(
        factory(StringView(MITE_NATIVE_KEYMAP_SOURCE, sizeof(MITE_NATIVE_KEYMAP_SOURCE) - 1),
            *keyboard, "mite:native-keymap/factory"sv)) };
    TRY(register_host_module("mite-native-keymap", *keymap,
        { "getCurrentKeyboardLayout", "getKeyMap", "isISOKeyboard", "onDidChangeKeyboardLayout" }));
    GC::Root copy { create_file_clone_host(m_realm) };
    GC::Root filesystem { TRY(builtin("fs")) };
    for (auto name : { "copyFile"_utf16, "copyFileSync"_utf16, "constants"_utf16 })
        copy->define_direct_property(name, TRY(filesystem->get(name)), JS::default_attributes);
    TRY(register_host_module("mite-fs-copyfile", *copy,
        { "copyFile", "copyFileSync", "constants", "isCloneSupported" }));
    GC::Root host { JS::Object::create(m_realm, nullptr) };
    GC::Root sqlite { SQLite::create_host(m_realm, m_event_loop) };
    host->define_direct_property("native"_utf16, sqlite.ptr(), JS::default_attributes);
    host->define_direct_property(
        "EventEmitter"_utf16, TRY(builtin("events")), JS::default_attributes);
    GC::Root buffer { TRY(builtin("buffer")) };
    host->define_direct_property(
        "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    GC::Root exports { TRY(factory(StringView(MITE_SQLITE_SOURCE, sizeof(MITE_SQLITE_SOURCE) - 1),
        *host, "mite:sqlite/factory"sv)) };
    return register_host_module("mite-sqlite", *exports,
        { "Database", "Statement", "OPEN_READONLY", "OPEN_READWRITE", "OPEN_CREATE", "VERSION",
            "verbose" });
}
}
