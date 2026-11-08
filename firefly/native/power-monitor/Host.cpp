/** Trusted synchronous observations and weak, owner-thread delivery of passive native events. */
#include "Host.h"
#include "Provider.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <optional>
#include <unordered_set>

namespace Firefly::PowerMonitor {
struct Controller::State {
    std::mutex mutex;
    QObject* receiver {};
    std::function<void(std::string)> notify;
    bool stopped { false };
    uint64_t epoch {}, last_epoch {};
    std::shared_ptr<Provider> provider;
    std::optional<std::vector<std::string>> capabilities;
};

namespace {
    bool known(std::string const& type)
    {
        static std::unordered_set<std::string> const names { "suspend", "resume", "on-ac",
            "on-battery", "thermal-state-change", "speed-limit-change", "shutdown", "lock-screen",
            "unlock-screen", "user-did-become-active", "user-did-resign-active" };
        return names.contains(type);
    }

    JS::Completion fail(JS::VM& vm, std::string const& code, std::string const& message)
    {
        GC::Root error { JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message.data(), message.size()))) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(code.data(), code.size()))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<uint64_t> epoch(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
            || value.as_double() > 2147483647 || std::floor(value.as_double()) != value.as_double())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid power observer epoch");
        return static_cast<uint64_t>(value.as_double());
    }

    JS::ThrowCompletionOr<JS::Value> json(JS::VM& vm, QJsonDocument const& document)
    {
        auto bytes = document.toJson(QJsonDocument::Compact);
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
    }

    JS::ThrowCompletionOr<std::vector<std::string>> capabilities(
        JS::VM& vm, Controller::State& state)
    {
        if (state.capabilities)
            return *state.capabilities;
        auto names = Provider::capabilities();
        std::unordered_set<std::string> seen;
        if (names.size() > 11)
            return fail(vm, "ERR_FIREFLY_POWER_MONITOR_DATA", "Too many native power capabilities");
        for (auto const& name : names)
            if (!known(name) || !seen.insert(name).second)
                return fail(
                    vm, "ERR_FIREFLY_POWER_MONITOR_DATA", "Invalid native power capabilities");
        state.capabilities = names;
        return names;
    }

    void post(std::weak_ptr<Controller::State> weak, uint64_t id, Event event)
    {
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        if (state->stopped || !state->receiver)
            return;
        QMetaObject::invokeMethod(
            state->receiver,
            [weak, id, event = std::move(event)] {
                auto current = weak.lock();
                if (!current || current->stopped || current->epoch != id)
                    return;
                // Native availability is snapshotted once per host. A later backend
                // recovery cannot invent events that the public API did not advertise.
                if (known(event.type) && current->capabilities
                    && std::find(
                           current->capabilities->begin(), current->capabilities->end(), event.type)
                        == current->capabilities->end())
                    return;
                QJsonObject value { { "epoch", static_cast<qint64>(id) } };
                if (event.type.size() > 128 || event.state.size() > 128) {
                    value["type"] = "invalid-native-event";
                } else {
                    value["type"] = QString::fromStdString(event.type);
                    value["state"] = QString::fromStdString(event.state);
                    value["limit"] = event.limit;
                }

                auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
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
        m_state->epoch = 0;
        m_state->notify = {};
        provider = std::move(m_state->provider);
    }

    if (provider)
        provider->stop();
}

std::shared_ptr<Controller> install_host(
    JS::Realm& realm, JS::Object& host, QObject& receiver, std::function<void(std::string)> notify)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    host.define_native_function(
        realm, "powerMonitorCapabilities"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (state->stopped)
                return fail(vm, "ERR_FIREFLY_APP_EXITED", "Power monitor stopped");
            QJsonArray names;
            for (auto const& name : TRY(capabilities(vm, *state)))
                names.append(QString::fromStdString(name));
            return json(vm, QJsonDocument(names));
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "powerMonitorQuery"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (state->stopped)
                return fail(vm, "ERR_FIREFLY_APP_EXITED", "Power monitor stopped");
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a power query name");
            auto name = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
            auto kind = name == "idle-time"sv ? Query::IdleTime
                : name == "idle-state"sv      ? Query::IdleState
                : name == "thermal-state"sv   ? Query::ThermalState
                : name == "battery-power"sv   ? Query::BatteryPower
                                              : Query::SpeedLimit;
            if (kind == Query::SpeedLimit && name != "speed-limit"sv)
                return fail(vm, "ERR_INVALID_ARG_VALUE", "Unknown power query");
            int threshold = 60;
            if (kind == Query::IdleState) {
                auto value = vm.argument(1);
                if (!value.is_number() || !std::isfinite(value.as_double())
                    || std::floor(value.as_double()) != value.as_double())
                    return fail(vm, "ERR_INVALID_ARG_TYPE", "Idle threshold must be an integer");
                if (value.as_double() <= 0 || value.as_double() > 2147483647)
                    return fail(vm, "ERR_OUT_OF_RANGE", "Idle threshold out of range");
                threshold = static_cast<int>(value.as_double());
            }

            auto result = query(kind, threshold);
            if (!result.code.empty())
                return fail(vm, result.code, result.message);
            QJsonValue value;
            if (kind == Query::IdleTime || kind == Query::SpeedLimit) {
                if (!std::isfinite(result.number) || result.number < 0
                    || result.number > (kind == Query::IdleTime ? 9007199254740991.0 : 100.0)
                    || std::floor(result.number) != result.number)
                    return fail(
                        vm, "ERR_FIREFLY_POWER_MONITOR_DATA", "Invalid native power number");
                value = result.number;
            } else if (kind == Query::BatteryPower) {
                value = result.flag;
            } else {
                static std::unordered_set<std::string> const idle { "active", "idle", "locked",
                    "unknown" };

                static std::unordered_set<std::string> const thermal { "unknown", "nominal", "fair",
                    "serious", "critical" };
                if (!(kind == Query::IdleState ? idle : thermal).contains(result.text))
                    return fail(vm, "ERR_FIREFLY_POWER_MONITOR_DATA", "Invalid native power state");
                value = QString::fromStdString(result.text);
            }

            return json(vm, QJsonDocument(QJsonObject { { "value", value } }));
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "powerMonitorStart"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto id = TRY(epoch(vm));
            if (state->stopped)
                return fail(vm, "ERR_FIREFLY_APP_EXITED", "Power monitor stopped");
            if (TRY(capabilities(vm, *state)).empty())
                return fail(
                    vm, "ERR_FIREFLY_POWER_UNAVAILABLE", "No native power events available");
            if (id <= state->last_epoch || state->epoch)
                return fail(
                    vm, "ERR_FIREFLY_POWER_MONITOR_LIMIT", "Power epoch reused or still active");
            state->last_epoch = id;
            state->epoch = id;
            auto provider = Provider::create([weak = std::weak_ptr(state), id](Event event) {
                post(weak, id, std::move(event));
            });
            state->provider = provider;
            auto result = provider ? provider->start()
                                   : Result { 0, {}, false, "ERR_FIREFLY_POWER_UNAVAILABLE",
                                         "Native power observers unavailable" };

            if (!result.code.empty()) {
                state->epoch = 0;
                state->provider.reset();
                if (provider)
                    provider->stop();
                return fail(vm, result.code, result.message);
            }

            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "powerMonitorStop"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto id = TRY(epoch(vm));
            if (state->epoch == id) {
                state->epoch = 0;
                auto provider = std::move(state->provider);
                if (provider)
                    provider->stop();
            }

            return JS::js_undefined();
        },
        1, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
