/** Marshal plain OS results onto the main owner before queuing the established VM bridge event. */
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
#include <unordered_set>

namespace Firefly::DesktopCapture {
struct Controller::State {
    // Native provider IDs/metadata only; notify is an owner-scoped native transport.
    using HoldsNoCellEdges = void;
    // The mutex protects only posting versus owner teardown. JS values never live
    // here. Pending IDs and notify are accessed on the QObject owner thread only.
    std::mutex mutex;
    QObject* receiver {};
    std::function<void(std::string)> notify;
    std::unordered_set<int> pending;
    bool stopped { false };
    int last_id { 0 };
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

    JS::ThrowCompletionOr<int> request_id(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || value.as_double() < 1 || value.as_double() > 2147483647
            || std::floor(value.as_double()) != value.as_double())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid desktop capture request ID");
        return static_cast<int>(value.as_double());
    }

    void post(std::weak_ptr<Controller::State> weak, int id, Result result)
    {
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        if (state->stopped || !state->receiver)
            return;
        // QObject deletion is owner-only and follows stop() under this mutex.
        // A queued completion contains only native data and a weak state token.
        QMetaObject::invokeMethod(
            state->receiver,
            [weak, id, result = std::move(result)] {
                auto current = weak.lock();
                if (!current || current->stopped || !current->pending.erase(id))
                    return;
                QJsonObject payload { { "id", id } };
                if (!result.code.empty()) {
                    payload["code"] = QString::fromStdString(result.code);
                    payload["message"] = QString::fromStdString(result.message);
                } else {
                    QJsonArray sources;
                    for (auto const& source : result.sources)
                        sources.append(QJsonObject { { "id", QString::fromStdString(source.id) },
                            { "name", QString::fromStdString(source.name) },
                            { "display_id", QString::fromStdString(source.display_id) } });
                    payload["sources"] = sources;
                }

                auto data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
                auto notify = current->notify;
                notify(std::string(data.constData(), data.size()));
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
    std::lock_guard lock(m_state->mutex);
    m_state->stopped = true;
    m_state->receiver = nullptr;
    m_state->notify = {};
    m_state->pending.clear();
}

std::shared_ptr<Controller> install_host(
    JS::Realm& realm, JS::Object& host, QObject& receiver, std::function<void(std::string)> notify)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    host.define_native_function(realm, "desktopCaptureStart"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto id = TRY(request_id(vm));
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Desktop capture dispatcher stopped");
                if (state->pending.size() >= 8)
                    return fail(
                        vm, "ERR_FIREFLY_CAPTURE_LIMIT", "Too many native desktop requests");
                if (id <= state->last_id || !state->pending.insert(id).second)
                    return fail(
                        vm, "ERR_FIREFLY_CAPTURE_DATA", "Reused desktop capture request ID");
                state->last_id = id;
                enumerate([weak = std::weak_ptr(state), id](
                              Result result) { post(weak, id, std::move(result)); });
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "desktopCaptureCancel"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                state->pending.erase(TRY(request_id(vm)));
                // Cancellation removes delivery authority, not the provider's OS slot.
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
