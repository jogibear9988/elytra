/** Private snapshots never publish native widgets/paths to renderer realms. */
#include "Host.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringDecoder>
#include <QWidget>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Firefly::Dialogs {
struct Controller::State {
    struct Pending {
        int parent_id {};
        QPointer<QWidget> parent;
        QMetaObject::Connection destroyed;
        bool queued {};
    };

    std::mutex mutex;
    QObject* receiver {};
    std::function<void(std::string)> notify;
    ParentResolver resolve;
    ProviderFactory factory;
    std::unique_ptr<Provider> provider;
    std::unordered_map<int, Pending> pending;
    std::vector<std::pair<int, Result>> deferred;
    int last_id {};
    bool stopped {}, synchronous {}, initializing {};

    void stop()
    {
        {
            std::lock_guard lock(mutex);
            if (stopped)
                return;
            stopped = true;
            receiver = nullptr;
            notify = {};
            resolve = {};
            for (auto& [id, request] : pending) {
                (void)id;
                QObject::disconnect(request.destroyed);
            }

            pending.clear();
            deferred.clear();
        }

        if (provider)
            provider->stop();
    }
};

namespace {
    Result error(char const* code, char const* message)
    {
        return { {}, code, message };
    }

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

    JS::ThrowCompletionOr<int> integer(JS::VM& vm, JS::Value value, bool zero = false)
    {
        if (!value.is_number() || !std::isfinite(value.as_double())
            || value.as_double() < (zero ? 0 : 1) || value.as_double() > 2147483647
            || std::floor(value.as_double()) != value.as_double())
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid dialog ID");
        return static_cast<int>(value.as_double());
    }

    bool valid_text(QString const& text, int maximum = 32768)
    {
        if (text.toUtf8().size() > maximum)
            return false;
        for (qsizetype i = 0; i < text.size(); ++i) {
            if (text[i].isNull() || text[i].isLowSurrogate())
                return false;
            if (text[i].isHighSurrogate() && (++i == text.size() || !text[i].isLowSurrogate()))
                return false;
        }

        return true;
    }

    JS::ThrowCompletionOr<QString> raw_text(JS::VM& vm, JS::Value value, int maximum)
    {
        if (!value.is_string())
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Dialog snapshot must be a string");
        auto view = value.as_string().utf16_string_view();
        if (view.length_in_code_units() > static_cast<size_t>(maximum) || !view.validate())
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid dialog snapshot");
        auto bytes = MUST(view.to_byte_string());
        if (bytes.length() > static_cast<size_t>(maximum))
            return fail(vm, "ERR_OUT_OF_RANGE", "Dialog snapshot exceeds its budget");
        return QString::fromUtf8(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<QJsonObject> options(JS::VM& vm, JS::Value value, QString const& kind)
    {
        auto source = TRY(raw_text(vm, value, 262144));
        QJsonParseError parse;
        auto document = QJsonDocument::fromJson(source.toUtf8(), &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject())
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid dialog JSON object");
        auto object = document.object();
        QStringList common { "title" };
        QStringList strings = common;
        QStringList booleans;
        QStringList allowed;
        if (kind == "message") {
            strings += { "message", "detail", "type", "checkboxLabel" };
            booleans = { "checkboxChecked", "noLink", "normalizeAccessKeys" };
            allowed = { "buttons", "defaultId", "cancelId" };
        } else if (kind == "open" || kind == "save") {
            strings += { "defaultPath", "buttonLabel", "message" };
            if (kind == "save") {
                strings.append("nameFieldLabel");
                booleans.append("showsTagField");
            }

            allowed = { "filters", "properties" };
        } else {
            return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid dialog kind");
        }

        allowed += strings;
        allowed += booleans;
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!allowed.contains(it.key()))
                return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API", "Unsupported native dialog option");
            if (strings.contains(it.key())
                && (!it.value().isString() || !valid_text(it.value().toString())))
                return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog text");
            if (booleans.contains(it.key()) && !it.value().isBool())
                return fail(vm, "ERR_INVALID_ARG_TYPE", "Invalid native dialog flag");
        }

        if (kind == "message") {
            auto buttons = object.value("buttons");
            if (!buttons.isArray() || buttons.toArray().isEmpty() || buttons.toArray().size() > 32)
                return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog buttons");
            for (auto const& button : buttons.toArray())
                if (!button.isString() || !valid_text(button.toString(), 1024))
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog button");
            for (auto name : { "defaultId", "cancelId" }) {
                auto index = object.value(name);
                if (!index.isDouble() || index.toDouble() != std::floor(index.toDouble())
                    || index.toDouble() < (QString(name) == "defaultId" ? -1 : 0)
                    || index.toDouble() >= buttons.toArray().size())
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog button index");
            }

            if (!QStringList { "none", "info", "error", "question", "warning" }.contains(
                    object.value("type").toString()))
                return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog type");
        } else {
            if (object.contains("properties")) {
                if (!object.value("properties").isArray()
                    || object.value("properties").toArray().size() > 16)
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog properties");
                QStringList supported = kind == "open"
                    ? QStringList { "openFile", "openDirectory", "multiSelections",
                          "showHiddenFiles", "createDirectory", "noResolveAliases",
                          "treatPackageAsDirectory" }

                    : QStringList { "showHiddenFiles", "createDirectory",
                          "treatPackageAsDirectory" };

                for (auto const& property : object.value("properties").toArray())
                    if (!property.isString() || !supported.contains(property.toString()))
                        return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API",
                            "Unsupported native dialog property");
            }

            if (kind == "open") {
                auto properties = object.value("properties").toArray();
                if (!properties.contains("openFile") && !properties.contains("openDirectory"))
                    return fail(vm, "ERR_INVALID_ARG_VALUE",
                        "Open dialog requires a file or directory selector");
            }

            if (object.contains("filters")) {
                if (!object.value("filters").isArray()
                    || object.value("filters").toArray().size() > 64)
                    return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog filters");
                for (auto const& entry : object.value("filters").toArray()) {
                    auto filter = entry.toObject();
                    if (!entry.isObject() || filter.size() != 2 || !filter.value("name").isString()
                        || !valid_text(filter.value("name").toString())
                        || !filter.value("extensions").isArray()
                        || filter.value("extensions").toArray().isEmpty()
                        || filter.value("extensions").toArray().size() > 64)
                        return fail(vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog filter");
                    for (auto const& extension : filter.value("extensions").toArray()) {
                        auto text = extension.toString();
                        if (!extension.isString() || text.isEmpty() || !valid_text(text, 256)
                            || (text != "*"
                                && (text.contains('.') || text.contains('*') || text.contains('/')
                                    || text.contains('\\'))))
                            return fail(
                                vm, "ERR_INVALID_ARG_VALUE", "Invalid native dialog extension");
                    }
                }
            }
        }

        return object;
    }

    Result checked_result(Result result)
    {
        for (auto const* item : { &result.code, &result.message }) {
            QStringDecoder decode(QStringDecoder::Utf8);
            QString value = decode(QByteArrayView(item->data(), item->size()));
            if (decode.hasError() || !valid_text(value, item == &result.code ? 256 : 32768))
                return error("ERR_FIREFLY_DIALOG_DATA", "Invalid native dialog error");
        }

        if (QJsonDocument(result.value).toJson(QJsonDocument::Compact).size() > 262000)
            return error("ERR_FIREFLY_DIALOG_DATA", "Native dialog result exceeds its budget");
        return result;
    }

    void post(std::weak_ptr<Controller::State> weak, int id, Result result)
    {
        result = checked_result(std::move(result));
        auto state = weak.lock();
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        auto found = state->pending.find(id);
        if (state->stopped || !state->receiver || found == state->pending.end()
            || found->second.queued)
            return;
        found->second.queued = true;
        QMetaObject::invokeMethod(
            state->receiver,
            [weak, id, result = std::move(result)]() mutable {
                auto current = weak.lock();
                if (!current)
                    return;
                std::function<void(std::string)> notify;
                Controller::State::Pending request;
                {
                    std::lock_guard lock(current->mutex);
                    auto found = current->pending.find(id);
                    if (current->stopped || found == current->pending.end())
                        return;
                    // A native modal loop must never reenter this Host's JS sink.
                    if (current->synchronous) {
                        current->deferred.emplace_back(id, std::move(result));
                        return;
                    }

                    request = found->second;
                    current->pending.erase(found);
                    QObject::disconnect(request.destroyed);
                    notify = current->notify;
                }

                if (request.parent_id
                    && (!request.parent || !current->resolve
                        || current->resolve(request.parent_id) != request.parent.data()))
                    result = error("ERR_FIREFLY_OBJECT_DESTROYED", "Dialog parent closed");
                QJsonObject event { { "id", id } };
                if (!result.code.empty())
                    event.insert("error",
                        QJsonObject {
                            { "code", QString::fromUtf8(result.code.data(), result.code.size()) },
                            { "message",
                                QString::fromUtf8(
                                    result.message.data(), result.message.size()) } });
                else
                    event.insert("value", result.value);
                auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact);
                if (bytes.size() > 262144)
                    bytes = QJsonDocument(
                        QJsonObject { { "id", id },
                            { "error",
                                QJsonObject { { "code", "ERR_FIREFLY_DIALOG_DATA" },
                                    { "message", "Dialog result exceeds its budget" } } } })
                                .toJson(QJsonDocument::Compact);
                if (notify)
                    notify(std::string(bytes.constData(), bytes.size()));
            },
            Qt::QueuedConnection);
    }

    JS::ThrowCompletionOr<Provider*> provider(JS::VM& vm, Controller::State& state)
    {
        if (state.stopped)
            return fail(vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped");
        if (!state.provider) {
            if (state.initializing)
                return fail(vm, "ERR_FIREFLY_DIALOG_LIMIT",
                    "Dialog provider initialization is in progress");
            state.initializing = true;
            state.provider = state.factory ? state.factory() : Provider::create();
            state.initializing = false;
        }

        if (state.stopped) {
            if (state.provider)
                state.provider->stop();
            return fail(vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped during initialization");
        }

        if (!state.provider)
            return fail(vm, "ERR_FIREFLY_DIALOG_UNAVAILABLE", "Native dialogs unavailable");
        return state.provider.get();
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

bool Controller::is_synchronous() const
{
    return m_state->synchronous;
}

bool Controller::has_pending() const
{
    return !m_state->pending.empty();
}

std::shared_ptr<Controller> install_host(JS::Realm& realm, JS::Object& host, QObject& receiver,
    std::function<void(std::string)> notify, ParentResolver resolve, ProviderFactory factory)
{
    auto state = std::make_shared<Controller::State>();
    state->receiver = &receiver;
    state->notify = std::move(notify);
    state->resolve = std::move(resolve);
    state->factory = std::move(factory);
    QObject::connect(&receiver, &QObject::destroyed, [weak = std::weak_ptr(state)] {
        if (auto current = weak.lock())
            current->stop();
    });
    host.define_native_function(
        realm, "dialogStart"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto id = TRY(integer(vm, vm.argument(0)));
            auto kind = TRY(raw_text(vm, vm.argument(1), 16));
            auto parent_id = TRY(integer(vm, vm.argument(2), true));
            if (state->stopped)
                return fail(vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped");
            auto snapshot = TRY(options(vm, vm.argument(3), kind));
            QPointer<QWidget> parent
                = parent_id && state->resolve ? state->resolve(parent_id) : nullptr;
            if (parent_id && !parent)
                return fail(vm, "ERR_FIREFLY_OBJECT_DESTROYED", "Unknown dialog parent");
            {
                std::lock_guard lock(state->mutex);
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped");
                if (state->synchronous || id <= state->last_id || state->pending.size() >= 16)
                    return fail(
                        vm, "ERR_FIREFLY_DIALOG_LIMIT", "Dialog admission budget exhausted");
                state->last_id = id;
            }

            auto* native = TRY(provider(vm, *state));
            if (parent_id && (!parent || state->resolve(parent_id) != parent.data()))
                return fail(
                    vm, "ERR_FIREFLY_OBJECT_DESTROYED", "Dialog parent closed during admission");
            {
                std::lock_guard lock(state->mutex);
                state->pending.emplace(
                    id, Controller::State::Pending { parent_id, parent, {}, false });
            }

            if (parent) {
                auto connection = QObject::connect(
                    parent.data(), &QObject::destroyed, [weak = std::weak_ptr(state), id] {
                        if (auto current = weak.lock()) {
                            post(weak, id,
                                error("ERR_FIREFLY_OBJECT_DESTROYED", "Dialog parent closed"));
                            if (current->provider)
                                current->provider->cancel(id);
                        }
                    });
                std::lock_guard lock(state->mutex);
                state->pending.at(id).destroyed = connection;
            }

            auto result = native->start(id, kind, snapshot, parent.data(),
                [weak = std::weak_ptr(state), id](
                    Result completion) { post(weak, id, std::move(completion)); });
            if (!result.code.empty() || state->stopped) {
                {
                    std::lock_guard lock(state->mutex);
                    auto found = state->pending.find(id);
                    if (found != state->pending.end()) {
                        QObject::disconnect(found->second.destroyed);
                        state->pending.erase(found);
                    }
                }

                native->cancel(id);
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped during admission");
                return fail(vm, result.code, result.message);
            }

            return JS::js_undefined();
        },
        4, JS::default_attributes);
    host.define_native_function(
        realm, "dialogMessageSync"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto parent_id = TRY(integer(vm, vm.argument(0), true));
            if (state->stopped)
                return fail(vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped");
            auto snapshot = TRY(options(vm, vm.argument(1), "message"));
            QPointer<QWidget> parent
                = parent_id && state->resolve ? state->resolve(parent_id) : nullptr;
            if (parent_id && !parent)
                return fail(vm, "ERR_FIREFLY_OBJECT_DESTROYED", "Unknown dialog parent");
            auto* native = TRY(provider(vm, *state));
            if (state->synchronous)
                return fail(
                    vm, "ERR_FIREFLY_DIALOG_LIMIT", "Nested synchronous dialogs unavailable");
            if (parent_id && (!parent || state->resolve(parent_id) != parent.data()))
                return fail(
                    vm, "ERR_FIREFLY_OBJECT_DESTROYED", "Dialog parent closed during admission");
            state->synchronous = true;
            auto result = checked_result(native->message_sync(snapshot, parent.data()));
            state->synchronous = false;
            auto deferred = std::move(state->deferred);
            state->deferred.clear();
            for (auto& [id, completion] : deferred) {
                {
                    std::lock_guard lock(state->mutex);
                    auto found = state->pending.find(id);
                    if (found != state->pending.end())
                        found->second.queued = false;
                }

                post(std::weak_ptr(state), id, std::move(completion));
            }

            if (state->stopped)
                return fail(
                    vm, "ERR_FIREFLY_APP_EXITED", "Dialogs stopped during native modal operation");
            if (parent_id && (!parent || state->resolve(parent_id) != parent.data()))
                return fail(vm, "ERR_FIREFLY_OBJECT_DESTROYED",
                    "Dialog parent closed during modal operation");
            if (!result.code.empty())
                return fail(vm, result.code, result.message);
            auto bytes = QJsonDocument(result.value).toJson(QJsonDocument::Compact);
            if (bytes.size() > 262144)
                return fail(vm, "ERR_OUT_OF_RANGE", "Dialog result exceeds its budget");
            return JS::PrimitiveString::create(vm,
                Utf16String::from_utf8_with_replacement_character(
                    StringView(bytes.constData(), bytes.size())));
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "dialogCancel"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto id = TRY(integer(vm, vm.argument(0)));
            {
                std::lock_guard lock(state->mutex);
                auto found = state->pending.find(id);
                if (found == state->pending.end())
                    return JS::js_undefined();
                QObject::disconnect(found->second.destroyed);
                state->pending.erase(found);
            }

            if (state->provider)
                state->provider->cancel(id);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "dialogShutdown"_utf16,
        [state](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            state->stop();
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
