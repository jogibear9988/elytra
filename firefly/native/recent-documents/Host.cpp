#include "Host.h"
#include "../platform/GCIncludes.h"
#include "Provider.h"
#include "modules/ModulePaths.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QObject>
#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>

namespace Firefly::RecentDocuments {
namespace {
    struct State {
        std::thread::id owner { std::this_thread::get_id() };
        std::atomic<bool> stopped {};

        void visit_capture_edges(GC::Cell::Visitor&) const
        {
        }
    };

    JS::Completion fail(JS::VM& vm, StringView code, StringView message)
    {
        GC::Root error { JS::Error::create(vm.realm(), Utf16String::from_utf8(message)) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<JS::Value> complete(JS::VM& vm, Result const& result)
    {
        if (!result.code.empty())
            return fail(vm, StringView { result.code.data(), result.code.size() },
                StringView { result.message.data(), result.message.size() });
        return JS::js_undefined();
    }

    JS::ThrowCompletionOr<void> check(JS::VM& vm, State const& state)
    {
        if (state.owner != std::this_thread::get_id())
            return fail(vm, "ERR_FIREFLY_RECENT_DOCUMENTS_THREAD"sv,
                "Recent documents require the owner thread"sv);
        if (state.stopped)
            return fail(vm, "ERR_FIREFLY_APP_EXITED"sv, "Recent documents host has stopped"sv);
        if (!available())
            return fail(
                vm, "ERR_FIREFLY_UNSUPPORTED_API"sv, "Native recent documents are unavailable"sv);
        return {};
    }
}

void install_host(JS::Realm& realm, JS::Object& host, QObject& context)
{
    auto state = std::make_shared<State>();
    QObject::connect(&context, &QObject::destroyed, [weak = std::weak_ptr<State>(state)] {
        if (auto current = weak.lock())
            current->stopped = true;
    });
    host.define_native_function(realm, "recentDocumentsClear"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                TRY(check(vm, *state));
                if (vm.argument_count() != 0)
                    return fail(vm, "ERR_INVALID_ARG_TYPE"sv,
                        "clearRecentDocuments expects no arguments"sv);
                return complete(vm, clear());
            }),
        0, JS::default_attributes);
    host.define_native_function(realm, "recentDocumentsAdd"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                TRY(check(vm, *state));
                if (vm.argument_count() != 1 || !vm.argument(0).is_string())
                    return fail(vm, "ERR_INVALID_ARG_TYPE"sv,
                        "addRecentDocument expects one path string"sv);
                auto view = vm.argument(0).as_string().utf16_string_view();
                if (view.is_empty() || view.length_in_code_units() > 32768)
                    return fail(vm, "ERR_INVALID_ARG_VALUE"sv,
                        "Recent document path is empty or too long"sv);
                auto text = view.to_utf8(AllowLonelySurrogates::No);
                if (text.is_error() || text.value().contains('\0')
                    || text.value().byte_count() > 32768)
                    return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid recent document path"sv);
                auto path = Mite::ModulePaths::path_from_utf8(std::string(
                    text.value().bytes_as_string_view().characters_without_null_termination(),
                    text.value().byte_count()));
                if (!path.is_absolute()) {
                    std::error_code failure;
                    auto cwd = std::filesystem::current_path(failure);
                    if (failure)
                        return fail(vm, "ERR_FIREFLY_RECENT_DOCUMENTS_PATH"sv,
                            "Cannot resolve recent document path"sv);
                    path = cwd / path;
                }

                auto absolute = Mite::ModulePaths::path_string(path);
                if (absolute.size() > 32768)
                    return fail(vm, "ERR_INVALID_ARG_VALUE"sv,
                        "Resolved recent document path is too long"sv);
                return complete(vm, add(absolute));
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "recentDocumentsStop"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (state->owner != std::this_thread::get_id())
                    return fail(vm, "ERR_FIREFLY_RECENT_DOCUMENTS_THREAD"sv,
                        "Recent documents require the owner thread"sv);
                state->stopped = true;
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
}
}
