/** Validate private registration input and retire ownership before queued events reach the VM. */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <cmath>
#include <mutex>
#include <unordered_set>

namespace Firefly::GlobalShortcut {
struct Controller::State {
    // Native provider IDs/metadata only; notify is an owner-scoped native transport.
    using HoldsNoCellEdges = void;
    std::mutex mutex;
    QObject* receiver {};
    std::function<void(std::string)> notify;
    ProviderFactory factory;
    std::unique_ptr<Provider> provider;
    std::unordered_set<int> active, pending;
    int last_token {};
    bool stopped {};

    void stop()
    {
        {
            std::lock_guard lock(mutex);
            if (stopped)
                return;
            stopped = true;
            receiver = nullptr;
            notify = {};
            active.clear();
            pending.clear();
        }

        if (provider)
            provider->stop();
    }
};

namespace {
    JS::Completion fail(JS::VM& vm, std::string const& code, std::string const& message)
    {
        GC::Root value { JS::Error::create(vm.realm(),
            Utf16String::from_utf8_with_replacement_character(
                StringView(message.data(), message.size()))) };
        value->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm,
                Utf16String::from_utf8_with_replacement_character(
                    StringView(code.data(), code.size()))),
            JS::default_attributes);
        return JS::throw_completion(value);
    }

    JS::ThrowCompletionOr<int> token(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
            || value.as_double() > 2147483647 || std::floor(value.as_double()) != value.as_double())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid global shortcut token");
        return static_cast<int>(value.as_double());
    }

    void post(std::weak_ptr<Controller::State> weak, int id)
    {
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        if (state->stopped || !state->receiver || !state->active.contains(id)
            || !state->pending.insert(id).second)
            return;
        // Coalesce a held key while its owner is busy; at most 64 queued tokens.
        QMetaObject::invokeMethod(
            state->receiver,
            [weak, id] {
                auto current = weak.lock();
                if (!current)
                    return;
                std::function<void(std::string)> notify;
                {
                    std::lock_guard lock(current->mutex);
                    current->pending.erase(id);
                    if (current->stopped || !current->active.contains(id))
                        return;
                    notify = current->notify;
                }
                // This lambda runs only on the receiver's owner thread. Shutdown on
                // that same thread revokes state before destroying the receiver/VM.
                if (notify)
                    notify("{\"token\":" + std::to_string(id) + "}");
            },
            Qt::QueuedConnection);
    }
}

Controller::Controller(std::shared_ptr<State> state)
    : m_state(std::move(state))
{
}

Controller::~Controller()
{
    stop();
}

void Controller::stop()
{
    m_state->stop();
}

std::shared_ptr<Controller> install_host(JS::Realm& realm, JS::Object& host, QObject& receiver,
    std::function<void(std::string)> notify, ProviderFactory factory)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    state->factory = std::move(factory);
    // Receiver destruction revokes worker delivery before the VM/context is retired.
    QObject::connect(&receiver, &QObject::destroyed, [weak = std::weak_ptr(state)] {
        if (auto current = weak.lock())
            current->stop();
    });
    host.define_native_function(realm, "globalShortcutRegister"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto id = TRY(token(vm));
                auto key = vm.argument(1);
                auto modifiers = vm.argument(2);
                if (!key.is_string()
                    || key.as_string().utf16_string_view().length_in_code_units() > 64
                    || !modifiers.is_int32() || modifiers.as_i32() < 0 || modifiers.as_i32() > 15)
                    return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid global shortcut chord");
                if (!key.as_string().utf16_string_view().validate())
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid global shortcut UTF-16");
                auto encoded = MUST(key.as_string().utf16_string_view().to_byte_string());
                Chord chord { std::string(encoded.characters(), encoded.length()),
                    static_cast<unsigned>(modifiers.as_i32()) };
                if (chord.key.empty() || chord.key.find('\0') != std::string::npos)
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid global shortcut key");
                {
                    std::lock_guard lock(state->mutex);
                    if (state->stopped)
                        return fail(vm, "ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped");
                    if (id <= state->last_token || state->active.size() >= 64)
                        return fail(vm, "ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT",
                            "Global shortcut token or capacity exhausted");
                    state->last_token = id;
                }

                if (!state->provider) {
#ifdef __APPLE__
                    if (!state->factory
                        && QGuiApplication::platformName() != QStringLiteral("cocoa"))
                        return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API",
                            "Global shortcuts require the native Cocoa desktop backend");
#endif
                    auto callback = [weak = std::weak_ptr(state)](int token) { post(weak, token); };
                    state->provider = state->factory ? state->factory(std::move(callback))
                                                     : Provider::create(std::move(callback));
                    if (!state->provider)
                        return fail(vm, "ERR_FIREFLY_GLOBAL_SHORTCUT_UNAVAILABLE",
                            "Native global shortcut provider unavailable");
                }

                bool stopped_during_initialization;
                {
                    std::lock_guard lock(state->mutex);
                    stopped_during_initialization = state->stopped;
                    if (!stopped_during_initialization)
                        state->active.insert(id);
                }

                if (stopped_during_initialization) {
                    state->provider->stop();
                    return fail(vm, "ERR_FIREFLY_APP_EXITED",
                        "Global shortcuts stopped during initialization");
                }

                auto result = state->provider->register_shortcut(id, chord);
                if (!result.value || !result.code.empty()) {
                    std::lock_guard lock(state->mutex);
                    state->active.erase(id);
                    state->pending.erase(id);
                }

                if (!result.code.empty())
                    return fail(vm, result.code, result.message);
                if (state->stopped) {
                    if (result.value)
                        state->provider->unregister_shortcut(id);
                    return fail(
                        vm, "ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped during admission");
                }

                return JS::Value(result.value);
            }),
        3, JS::default_attributes);
    host.define_native_function(realm, "globalShortcutUnregister"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto id = TRY(token(vm));
                {
                    std::lock_guard lock(state->mutex);
                    if (state->stopped)
                        return fail(vm, "ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped");
                    if (!state->active.contains(id))
                        return JS::js_undefined();
                }

                auto result = state->provider->unregister_shortcut(id);
                if (!result.code.empty())
                    return fail(vm, result.code, result.message);
                {
                    std::lock_guard lock(state->mutex);
                    state->active.erase(id);
                    state->pending.erase(id);
                }

                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "globalShortcutShutdown"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                state->stop();
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
