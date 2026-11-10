/** Strict request snapshots and owner-thread delivery; OS callbacks never retain JS/GC values. */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include "Provider.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_set>

namespace Firefly::Shell {
struct Controller::State {
    // Native provider IDs/metadata only; notify is an owner-scoped native transport.
    using HoldsNoCellEdges = void;
    // Only posting versus receiver revocation shares this mutex. Provider,
    // pending IDs and callback delivery are confined to the QObject owner.
    std::mutex mutex;
    QObject* receiver {};
    std::function<void(std::string)> notify;
    std::unique_ptr<Provider> provider;
    std::unordered_set<int> pending;
    bool stopped { false };
    int last_id {};
};

namespace {
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

    JS::ThrowCompletionOr<int> request_id(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
            || value.as_double() > 2147483647 || std::floor(value.as_double()) != value.as_double())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid shell request ID");
        return static_cast<int>(value.as_double());
    }

    bool valid_text(QString const& value)
    {
        for (qsizetype index = 0; index < value.size(); ++index) {
            auto unit = value[index];
            if (unit.isNull() || unit.isLowSurrogate())
                return false;
            if (unit.isHighSurrogate()
                && (++index == value.size() || !value[index].isLowSurrogate()))
                return false;
        }

        return true;
    }

    JS::ThrowCompletionOr<QString> text(JS::VM& vm, QJsonValue value, size_t limit)
    {
        if (!value.isString())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected shell text");
        auto string = value.toString();
        if (string.isEmpty())
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Shell text must not be empty");
        if (!valid_text(string))
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Shell text contains NUL or invalid UTF-16");
        if (static_cast<size_t>(string.toUtf8().size()) > limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Shell text exceeds its UTF-8 budget");
        return string;
    }

    JS::ThrowCompletionOr<QByteArray> raw_text(JS::VM& vm, JS::Value value, size_t limit)
    {
        if (!value.is_string())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a shell string snapshot");
        auto view = value.as_string().utf16_string_view();
        if (view.length_in_code_units() > limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Shell snapshot exceeds its budget");
        if (!view.validate())
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Shell snapshot contains invalid UTF-16");
        auto bytes = MUST(view.to_byte_string());
        if (bytes.length() > limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Shell snapshot exceeds its UTF-8 budget");
        return QByteArray(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<std::string> path(JS::VM& vm, QString const& value)
    {
        // Capture cwd before asynchronous submission; do not canonicalize symlinks
        // or inspect the target, especially a symlink being sent to the trash.
        auto absolute
            = (QDir::isAbsolutePath(value) ? value : QDir::currentPath() + "/" + value).toUtf8();
        if (absolute.size() > 32768)
            return fail(vm, "ERR_OUT_OF_RANGE", "Absolute shell path exceeds its UTF-8 budget");
        return std::string(absolute.constData(), absolute.size());
    }

    JS::ThrowCompletionOr<Provider*> provider(JS::VM& vm, Controller::State& state)
    {
        if (!state.provider)
            state.provider = Provider::create();
        if (!state.provider)
            return fail(vm, "ERR_FIREFLY_SHELL_UNAVAILABLE", "Native desktop shell unavailable");
        return state.provider.get();
    }

    Result normalize(Result result)
    {
        if (result.code.size() > 128 || result.message.size() > 65536)
            return { "ERR_FIREFLY_SHELL_DATA", "Native shell result exceeds its budget" };
        auto code = QString::fromStdString(result.code);
        auto message = QString::fromStdString(result.message);
        bool code_valid = result.code.empty()
            || (result.code.front() >= 'A' && result.code.front() <= 'Z'
                && std::all_of(result.code.begin(), result.code.end(), [](char unit) {
                       return (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9')
                           || unit == '_';
                   }));
        if (!code_valid || code.toStdString() != result.code
            || message.toStdString() != result.message || !valid_text(code) || !valid_text(message)
            || (code.isEmpty() != message.isEmpty()))
            return { "ERR_FIREFLY_SHELL_DATA", "Malformed native shell result" };
        return result;
    }

    void post(std::weak_ptr<Controller::State> weak, int id, Result result)
    {
        // Normalize before queueing so both queued records and synchronous reveal
        // errors have the same bounded UTF-8 contract.
        result = normalize(std::move(result));
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        if (state->stopped || !state->receiver)
            return;
        QMetaObject::invokeMethod(
            state->receiver,
            [weak, id, result = std::move(result)] {
                auto current = weak.lock();
                if (!current || current->stopped || !current->pending.erase(id))
                    return;
                auto code = QString::fromStdString(result.code);
                auto message = QString::fromStdString(result.message);
                QJsonObject value { { "id", id }, { "ok", code.isEmpty() } };
                if (!code.isEmpty()) {
                    value["code"] = code;
                    value["message"] = message;
                }

                auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
                auto notify = current->notify;
                if (notify)
                    notify(std::string(bytes.constData(), bytes.size()));
            },
            Qt::QueuedConnection);
    }

    void stop(Controller::State& state)
    {
        std::unique_ptr<Provider> native;
        {
            std::lock_guard lock(state.mutex);
            state.stopped = true;
            state.receiver = nullptr;
            state.pending.clear();
            state.notify = {};
            native = std::move(state.provider);
        }
        // A provider can synchronously complete while stopping. Revocation and
        // releasing the posting lock precede native teardown and destruction.
        if (native)
            native->stop();
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
    Shell::stop(*m_state);
}

std::shared_ptr<Controller> install_host(
    JS::Realm& realm, JS::Object& host, QObject& receiver, std::function<void(std::string)> notify)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    host.define_native_function(realm, "shellShowItemInFolder"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Shell dispatcher stopped");
                auto source = TRY(raw_text(vm, vm.argument(0), 32768));
                auto target = TRY(path(vm, TRY(text(vm, QString::fromUtf8(source), 32768))));
                auto result = normalize(TRY(provider(vm, *state))->reveal(target));
                if (!result.code.empty())
                    return fail(vm, result.code, result.message);
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "shellRequest"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto id = TRY(request_id(vm));
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Shell dispatcher stopped");
                if (!vm.argument(1).is_string())
                    return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a shell operation");
                auto kind = TRY(raw_text(vm, vm.argument(1), 32));
                bool external = kind == "openExternal";
                if (!external && kind != "openPath" && kind != "trashItem")
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Unknown shell operation");
                auto source = TRY(raw_text(vm, vm.argument(2), 512 * 1024));
                auto document = QJsonDocument::fromJson(source);
                if (!document.isObject())
                    return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid shell request snapshot");
                auto options = document.object();
                for (auto const& key : options.keys())
                    if (key != (external ? "url" : "path") && !(external && key == "activate"))
                        return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API", "Unsupported shell option");
                bool activate = true;
                if (external) {
                    if (!options["activate"].isBool())
                        return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a shell activation flag");
                    activate = options["activate"].toBool();
                }

                auto value
                    = TRY(text(vm, options[external ? "url" : "path"], external ? 65536 : 32768));
                auto input = external ? value.toUtf8().toStdString() : TRY(path(vm, value));
                if (state->pending.size() >= 64)
                    return fail(
                        vm, "ERR_FIREFLY_SHELL_LIMIT", "Too many pending native shell requests");
                if (id <= state->last_id)
                    return fail(vm, "ERR_FIREFLY_SHELL_DATA", "Shell request ID reused");
                auto* native = TRY(provider(vm, *state));
                state->last_id = id;
                state->pending.insert(id);
                auto complete = [weak = std::weak_ptr(state), id](
                                    Result result) { post(weak, id, std::move(result)); };
                if (external)
                    native->open_external(std::move(input), activate, std::move(complete));
                else if (kind == "openPath")
                    native->open_path(std::move(input), std::move(complete));
                else
                    native->trash(std::move(input), std::move(complete));
                return JS::js_undefined();
            }),
        3, JS::default_attributes);
    host.define_native_function(realm, "shellCancel"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                state->pending.erase(TRY(request_id(vm)));
                // Delivery cancellation is not OS cancellation or a refunded OS slot.
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "shellShutdown"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                Shell::stop(*state);
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
