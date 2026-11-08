/** Typed native boundaries and bounded owner-thread delivery of passive notifications. */
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
#include <mutex>
#include <unordered_map>

namespace Firefly::SystemPreferences {
struct Controller::State {
    struct Subscription {
        std::string kind;
        unsigned queued {};
        bool failed {};
    };

    // Notification workers and owner admission/retirement share pending/queue
    // accounting under this lock. Provider and VM-facing notify remain owner-only.
    std::mutex mutex;
    QObject* receiver {};
    std::function<void(std::string)> notify;
    std::unique_ptr<Provider> provider;
    std::unordered_map<int, Subscription> pending;
    unsigned queued {};
    int last_id { -1 };
    bool stopped {};
};

namespace {
    constexpr char data_code[] = "ERR_FIREFLY_SYSTEM_PREFERENCES_DATA";

    JS::Completion fail(JS::VM& vm, std::string const& code, std::string const& message)
    {
        GC::Root value { JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message.data(), message.size()))) };
        value->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(code.data(), code.size()))),
            JS::default_attributes);
        return JS::throw_completion(value);
    }

    bool valid(QString const& value, bool allow_nul = false)
    {
        for (qsizetype index = 0; index < value.size(); ++index) {
            auto unit = value[index];
            if ((!allow_nul && unit.isNull()) || unit.isLowSurrogate())
                return false;
            if (unit.isHighSurrogate()
                && (++index == value.size() || !value[index].isLowSurrogate()))
                return false;
        }

        return true;
    }

    bool bounded(std::string const& value, size_t maximum)
    {
        if (value.size() > maximum)
            return false;
        auto converted = QString::fromStdString(value);
        return valid(converted) && converted.toStdString() == value;
    }

    Result normalize(Result result)
    {
        bool code_valid = result.code.empty()
            || (result.code[0] >= 'A' && result.code[0] <= 'Z'
                && std::all_of(result.code.begin(), result.code.end(), [](char unit) {
                       return (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9')
                           || unit == '_';
                   }));
        if (!code_valid || !bounded(result.code, 128) || !bounded(result.message, 65536)
            || result.code.empty() != result.message.empty())
            return { data_code, "Malformed native preference error" };
        return result;
    }

    JS::ThrowCompletionOr<QByteArray> raw(JS::VM& vm, JS::Value value, size_t limit)
    {
        if (!value.is_string())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected system preference text");
        auto view = value.as_string().utf16_string_view();
        if (view.length_in_code_units() > limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Preference snapshot exceeds its budget");
        if (!view.validate())
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid preference UTF-16");
        auto bytes = MUST(view.to_byte_string());
        if (bytes.length() > limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Preference snapshot exceeds its UTF-8 budget");
        return QByteArray(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<std::string> text(JS::VM& vm, JS::Value value, size_t limit)
    {
        auto bytes = TRY(raw(vm, value, limit));
        if (bytes.isEmpty() || bytes.contains('\0'))
            return fail(
                vm, "ERR_INVALID_ARG_VALUE", "Preference name must be nonempty without NUL");
        return bytes.toStdString();
    }

    JS::ThrowCompletionOr<std::string> type(JS::VM& vm)
    {
        auto value = TRY(text(vm, vm.argument(1), 64));
        if (value != "string" && value != "boolean")
            return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API", "Unsupported preference default type");
        return value;
    }

    JS::ThrowCompletionOr<std::string> kind(JS::VM& vm)
    {
        auto value = TRY(text(vm, vm.argument(1), 64));
        if (value != "distributed" && value != "local" && value != "workspace")
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid notification center kind");
        return value;
    }

    JS::ThrowCompletionOr<int> identity(JS::VM& vm, bool admission)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || !std::isfinite(value.as_double())
            || std::floor(value.as_double()) != value.as_double()
            || (value.as_double() == 0 && std::signbit(value.as_double())))
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Preference ID must be an int32");
        if (value.as_double() < (admission ? 0.0 : -2147483648.0)
            || value.as_double() > 2147483647.0)
            return fail(vm, "ERR_OUT_OF_RANGE", "Preference ID is out of range");
        return static_cast<int>(value.as_double());
    }

    JS::ThrowCompletionOr<Provider*> backend(JS::VM& vm, Controller::State& state)
    {
        if (state.stopped)
            return fail(vm, "ERR_FIREFLY_APP_EXITED", "Preference service stopped");
        if (!state.provider)
            state.provider = Provider::create();
        if (!state.provider)
            return fail(
                vm, "ERR_FIREFLY_SYSTEM_PREFERENCES_UNAVAILABLE", "Native preferences unavailable");
        return state.provider.get();
    }

    JS::ThrowCompletionOr<JS::Value> checked_value(
        JS::VM& vm, ValueResult result, std::string const& expected)
    {
        auto status = normalize({ result.code, result.message });
        if (!status.code.empty())
            return fail(vm, status.code, status.message);
        if (result.json.size() > 512 * 1024 || !bounded(result.json, 512 * 1024))
            return fail(vm, data_code, "Native preference snapshot exceeds its budget");
        auto document = QJsonDocument::fromJson(QByteArray(result.json.data(), result.json.size()));
        if (!document.isObject() || document.object().size() != 1
            || !document.object().contains("value"))
            return fail(vm, data_code, "Invalid native preference snapshot");
        QJsonValue value = document.object()["value"];
        if (expected == "boolean") {
            if (!value.isBool())
                return fail(vm, data_code, "Invalid native boolean preference");
        } else {
            auto string = value.toString();
            if (!value.isString() || !valid(string) || string.toUtf8().size() > 65536)
                return fail(vm, data_code, "Invalid native string preference");
            if (expected == "media" && string != "granted" && string != "denied"
                && string != "restricted" && string != "not-determined" && string != "unknown")
                return fail(vm, data_code, "Invalid native media authorization status");
        }
        // Serialize the validated value again, never expose unvalidated native JSON bytes.
        auto bytes
            = QJsonDocument(QJsonObject { { "value", value } }).toJson(QJsonDocument::Compact);
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
    }

    bool graph(QJsonValue const& value, unsigned depth, unsigned& nodes)
    {
        if (++nodes > 1024 || depth > 8)
            return false;
        if (value.isString())
            return valid(value.toString(), true);
        if (value.isDouble())
            return std::isfinite(value.toDouble());
        if (value.isArray()) {
            for (auto const& item : value.toArray())
                if (!graph(item, depth + 1, nodes))
                    return false;
        } else if (value.isObject()) {
            auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
                if (!valid(it.key(), true) || !graph(it.value(), depth + 1, nodes))
                    return false;
        }

        return !value.isUndefined();
    }

    QJsonObject frame(Event const& event)
    {
        auto status = normalize({ event.code, event.message });
        QJsonObject result { { "id", event.id } };
        if (status.code.empty()) {
            if (event.name.empty() || !bounded(event.name, 4096) || !bounded(event.object, 4096)
                || !bounded(event.user_info_json, 65536)) {
                status = { data_code, "Invalid native notification text or size" };
            } else {
                auto document = QJsonDocument::fromJson(
                    QByteArray(event.user_info_json.data(), event.user_info_json.size()));
                unsigned nodes = 0;
                if (!document.isObject() || !graph(document.object(), 0, nodes))
                    status = { data_code, "Invalid native notification userInfo" };
                else {
                    result["event"] = QString::fromStdString(event.name);
                    result["object"] = QString::fromStdString(event.object);
                    result["userInfo"] = document.object();
                }
            }
        }

        if (!status.code.empty()) {
            result
                = QJsonObject { { "id", event.id }, { "code", QString::fromStdString(status.code) },
                      { "message", QString::fromStdString(status.message) } };
        }

        return result;
    }

    void post(std::weak_ptr<Controller::State> weak, int id, Event event)
    {
        event.id = id; // Provider-supplied IDs cannot redirect callback authority.
        auto payload = frame(event);
        bool terminal = payload.contains("code");
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        auto found = state->pending.find(id);
        if (state->stopped || !state->receiver || found == state->pending.end()
            || found->second.failed)
            return;
        if (!terminal && (state->queued >= 128 || found->second.queued >= 64)) {
            payload = QJsonObject { { "id", id }, { "code", data_code },
                { "message", "Native notification queue exhausted" } };
            terminal = true;
        }

        bool counted = !terminal;
        if (terminal)
            found->second.failed = true;
        else {
            ++state->queued;
            ++found->second.queued;
        }

        QMetaObject::invokeMethod(
            state->receiver,
            [weak, id, payload = std::move(payload), terminal, counted] {
                auto current = weak.lock();
                if (!current)
                    return;
                std::string center;
                std::function<void(std::string)> notify;
                {
                    std::lock_guard lock(current->mutex);
                    if (counted)
                        --current->queued;
                    auto found = current->pending.find(id);
                    if (found != current->pending.end() && counted)
                        --found->second.queued;
                    if (current->stopped || found == current->pending.end()
                        || (!terminal && found->second.failed))
                        return;
                    center = found->second.kind;
                    if (terminal)
                        current->pending.erase(found);
                    notify = current->notify;
                }

                if (terminal && current->provider)
                    current->provider->unsubscribe(id, center);
                auto bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
                if (notify)
                    notify(std::string(bytes.constData(), bytes.size()));
            },
            Qt::QueuedConnection);
    }

    void shutdown(Controller::State& state)
    {
        std::unique_ptr<Provider> provider;
        {
            std::lock_guard lock(state.mutex);
            state.stopped = true;
            state.receiver = nullptr;
            state.pending.clear();
            state.notify = {};
            provider = std::move(state.provider);
        }

        if (provider)
            provider->stop();
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
    shutdown(*m_state);
}

std::shared_ptr<Controller> install_host(
    JS::Realm& realm, JS::Object& host, QObject& receiver, std::function<void(std::string)> notify)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    host.define_native_function(
        realm, "preferencesGetDefault"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(text(vm, vm.argument(0), 4096));
            auto requested = TRY(type(vm));
            return checked_value(
                vm, TRY(backend(vm, *state))->get_user_default(name, requested), requested);
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "preferencesSetDefault"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(text(vm, vm.argument(0), 4096));
            auto requested = TRY(type(vm));
            auto bytes = TRY(raw(vm, vm.argument(2), 512 * 1024));
            auto document = QJsonDocument::fromJson("[" + bytes + "]");
            if (!document.isArray() || document.array().size() != 1)
                return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid preference value JSON");
            QJsonValue value = document.array()[0];
            if (requested == "boolean" ? !value.isBool()
                                       : (!value.isString() || !valid(value.toString())
                                             || value.toString().toUtf8().size() > 65536))
                return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid typed preference value");
            auto result = normalize(
                TRY(backend(vm, *state))->set_user_default(name, requested, bytes.toStdString()));
            if (!result.code.empty())
                return fail(vm, result.code, result.message);
            return JS::js_undefined();
        },
        3, JS::default_attributes);
    host.define_native_function(
        realm, "preferencesMediaStatus"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto requested = TRY(text(vm, vm.argument(0), 64));
            if (requested != "camera" && requested != "microphone" && requested != "screen")
                return fail(vm, "ERR_INVALID_ARG_VALUE", "Unknown media permission type");
            return checked_value(
                vm, TRY(backend(vm, *state))->media_access_status(requested), "media");
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "preferencesSubscribe"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto id = TRY(identity(vm, true));
            auto center = TRY(kind(vm));
            std::optional<std::string> name;
            if (!vm.argument(2).is_null())
                name = TRY(text(vm, vm.argument(2), 4096));
            auto* provider = TRY(backend(vm, *state));
            {
                std::lock_guard lock(state->mutex);
                if (state->pending.size() >= 64)
                    return fail(vm, "ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT",
                        "Too many preference subscriptions");
                if (id <= state->last_id)
                    return fail(vm, data_code, "Preference subscription ID reused");
                state->last_id = id;
                state->pending.emplace(id, Controller::State::Subscription { center, 0, false });
            }

            auto result = normalize(provider->subscribe(
                id, center, name, [weak = std::weak_ptr(state), id](Event event) {
                    post(weak, id, std::move(event));
                }));
            if (!result.code.empty()) {
                {
                    std::lock_guard lock(state->mutex);
                    state->pending.erase(id);
                }

                provider->unsubscribe(id, center);
                return fail(vm, result.code, result.message);
            }

            return JS::js_undefined();
        },
        3, JS::default_attributes);
    host.define_native_function(
        realm, "preferencesUnsubscribe"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto id = TRY(identity(vm, false));
            auto center = TRY(kind(vm));
            {
                std::lock_guard lock(state->mutex);
                auto found = state->pending.find(id);
                if (found == state->pending.end() || found->second.kind != center)
                    return JS::js_undefined();
                state->pending.erase(found);
            }

            if (state->provider) {
                auto result = normalize(state->provider->unsubscribe(id, center));
                if (!result.code.empty())
                    return fail(vm, result.code, result.message);
            }

            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "preferencesShutdown"_utf16,
        [state](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            shutdown(*state);
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
