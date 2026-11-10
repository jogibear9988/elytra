/** Validate native submissions and queue worker events onto the existing owner-thread VM bridge. */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include "Provider.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace Firefly::Notifications {
struct Controller::State {
    // Native provider IDs/metadata only; notify is an owner-scoped native transport.
    using HoldsNoCellEdges = void;

    struct Request {
        size_t actions;
        bool shown { false };
    };

    // Only posting/stop share this mutex. Provider/pending/notify stay owner-only;
    // no JS objects or VM roots are stored in state or native completion blocks.
    std::mutex mutex;
    QObject* receiver {};
    bool stopped { false };
    uint64_t last_token { 0 };
    std::unordered_map<uint64_t, Request> pending;
    std::shared_ptr<Provider> provider;
    std::function<void(std::string)> notify;
};

namespace {
    JS::Completion fail(JS::VM& vm, char const* code, char const* message)
    {
        GC::Root error { JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message)))) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<uint64_t> token(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || value.as_double() < 1 || value.as_double() > 2147483647
            || std::floor(value.as_double()) != value.as_double())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid notification token");
        return static_cast<uint64_t>(value.as_double());
    }

    JS::ThrowCompletionOr<std::string> text(JS::VM& vm, QJsonValue value, size_t limit)
    {
        if (!value.isString())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected notification text");
        auto string = value.toString();
        if (string.contains(QChar(0)))
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Notification text contains NUL");
        auto bytes = string.toUtf8();
        if (static_cast<size_t>(bytes.size()) > limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Notification UTF-8 text exceeds its budget");
        return std::string(bytes.constData(), bytes.size());
    }

    JS::ThrowCompletionOr<Options> options(JS::VM& vm)
    {
        auto value = vm.argument(1);
        if (!value.is_string())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a notification snapshot");
        auto source = MUST(value.as_string().utf16_string_view().to_byte_string());
        if (source.length() > 512 * 1024)
            return fail(vm, "ERR_OUT_OF_RANGE", "Notification snapshot exceeds its budget");
        auto document = QJsonDocument::fromJson(QByteArray(source.characters(), source.length()));
        if (!document.isObject())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid notification JSON");
        auto object = document.object();
        for (auto const& key : object.keys())
            if (key != "title" && key != "body" && key != "silent" && key != "actions")
                return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API", "Unsupported notification option");
        Options result;
        result.title = TRY(text(vm, object["title"], 4096));
        result.body = TRY(text(vm, object["body"], 65536));
        if (!object["silent"].isBool() || !object["actions"].isArray())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid notification flags or actions");
        result.silent = object["silent"].toBool();
        auto actions = object["actions"].toArray();
        if (actions.size() > 4)
            return fail(vm, "ERR_OUT_OF_RANGE", "Too many notification actions");
        for (auto const& action : actions) {
            if (!action.isObject())
                return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a notification action");
            auto item = action.toObject();
            if (item["type"].toString() != "button")
                return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API", "Only button actions are supported");
            for (auto const& key : item.keys())
                if (key != "type" && key != "text")
                    return fail(
                        vm, "ERR_FIREFLY_UNSUPPORTED_API", "Unsupported notification action");
            result.actions.push_back(TRY(text(vm, item["text"], 1024)));
        }

        return result;
    }

    void post(std::weak_ptr<Controller::State> weak, Event event)
    {
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        if (state->stopped || !state->receiver)
            return;
        QMetaObject::invokeMethod(
            state->receiver,
            [weak, event = std::move(event)]() mutable {
                auto current = weak.lock();
                if (!current || current->stopped)
                    return;
                auto found = current->pending.find(event.token);
                if (found == current->pending.end())
                    return;
                if ((event.type != "show" && event.type != "click" && event.type != "action"
                        && event.type != "close" && event.type != "failed")
                    || event.message.size() > 4096
                    || (event.type == "action"
                        && (event.index < 0
                            || static_cast<size_t>(event.index) >= found->second.actions))) {
                    event.type = "failed";
                    event.message = "Invalid native notification event";
                    event.index = -1;
                }

                if (event.type == "show") {
                    if (found->second.shown)
                        return;
                    found->second.shown = true;
                } else {
                    current->pending.erase(found);
                    auto provider = current->provider;
                    if (provider)
                        provider->close(event.token);
                }

                QJsonObject payload { { "token", static_cast<qint64>(event.token) },
                    { "type", QString::fromStdString(event.type) },
                    { "message", QString::fromStdString(event.message) },
                    { "index", event.index } };
                auto bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
                auto notify = current->notify;
                notify(std::string(bytes.constData(), bytes.size()));
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
    std::shared_ptr<Provider> provider;
    {
        std::lock_guard lock(m_state->mutex);
        m_state->stopped = true;
        m_state->receiver = nullptr;
        m_state->notify = {};
        m_state->pending.clear();
        provider = std::move(m_state->provider);
    }
    // Native stop can synchronously produce a callback; posting sees revoked state.
    if (provider)
        provider->stop();
}

std::shared_ptr<Controller> install_host(
    JS::Realm& realm, JS::Object& host, QObject& receiver, std::function<void(std::string)> notify)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    host.define_native_function(realm, "notificationSupported"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                return JS::Value(!state->stopped && Provider::supported());
            }),
        0, JS::default_attributes);
    host.define_native_function(realm, "notificationShow"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto id = TRY(token(vm));
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Notification dispatcher stopped");
                auto value = TRY(options(vm));
                if (state->pending.size() >= 64)
                    return fail(
                        vm, "ERR_FIREFLY_NOTIFICATION_LIMIT", "Too many active notifications");
                if (id <= state->last_token)
                    return fail(vm, "ERR_FIREFLY_NOTIFICATION_DATA", "Reused notification token");
                if (!state->provider)
                    state->provider = Provider::create([weak = std::weak_ptr(state)](Event event) {
                        post(weak, std::move(event));
                    });
                state->last_token = id;
                state->pending.emplace(id, Controller::State::Request { value.actions.size() });
                if (state->provider)
                    state->provider->show(id, std::move(value));
                else
                    post(
                        std::weak_ptr(state), { id, "failed", "Native notifications unavailable" });
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host.define_native_function(realm, "notificationClose"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto id = TRY(token(vm));
                state->pending.erase(id);
                if (state->provider)
                    state->provider->close(id);
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
