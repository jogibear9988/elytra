/** Native dialog registry and genuine Qt message boxes; no VM objects or HTML dialogs. */
#include "Provider.h"
#include "Operation.h"
#include <QApplication>
#include <QCheckBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <atomic>
#include <optional>
#include <unordered_map>

namespace Firefly::Dialogs {
namespace {
    Result failure(char const* code, char const* message)
    {
        return { {}, code, message };
    }

    bool owner_thread()
    {
        return QCoreApplication::instance()
            && QThread::currentThread() == QCoreApplication::instance()->thread();
    }

    bool text(QString const& value)
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

    Result validate(QString const& kind, QJsonObject const& options)
    {
        if (kind != "message" && kind != "open" && kind != "save")
            return failure("ERR_INVALID_ARG_VALUE", "Invalid native dialog kind");
        if (QJsonDocument(options).toJson(QJsonDocument::Compact).size() > 262144)
            return failure("ERR_OUT_OF_RANGE", "Dialog options exceed byte budget");
        QStringList allowed = kind == "message"
            ? QStringList { "type", "title", "message", "detail", "buttons", "defaultId",
                  "cancelId", "checkboxLabel", "checkboxChecked", "noLink", "normalizeAccessKeys" }
            : QStringList { "title", "defaultPath", "buttonLabel", "filters", "properties",
                  "message", "nameFieldLabel", "showsTagField" };
        for (auto it = options.begin(); it != options.end(); ++it) {
            if (!allowed.contains(it.key()))
                return failure(
                    "ERR_FIREFLY_UNSUPPORTED_API", "Native dialog option is unsupported");
            if (it.value().isString()
                && (!text(it.value().toString()) || it.value().toString().toUtf8().size() > 65536))
                return failure("ERR_INVALID_ARG_VALUE", "Invalid native dialog text");
        }

        if (kind == "message") {
            if (!QStringList { "none", "info", "error", "question", "warning" }.contains(
                    options.value("type").toString()))
                return failure("ERR_INVALID_ARG_VALUE", "Invalid message type");
            auto buttons = options.value("buttons");
            if (!buttons.isArray() || buttons.toArray().isEmpty() || buttons.toArray().size() > 64)
                return failure("ERR_INVALID_ARG_VALUE", "Invalid message buttons");
            for (auto item : buttons.toArray())
                if (!item.isString() || !text(item.toString())
                    || item.toString().toUtf8().size() > 4096)
                    return failure("ERR_INVALID_ARG_VALUE", "Invalid message button");
            auto count = buttons.toArray().size();
            for (auto name : { "defaultId", "cancelId" }) {
                auto value = options.value(name);
                auto minimum = QString(name) == "defaultId" ? -1 : 0;
                if (!value.isDouble() || value.toDouble() != value.toInt(-2)
                    || value.toInt() < minimum || value.toInt() >= count)
                    return failure("ERR_INVALID_ARG_VALUE", "Invalid message button index");
            }
        } else {
            if (!options.value("properties").isArray() || !options.value("filters").isArray())
                return failure("ERR_INVALID_ARG_VALUE", "Invalid file dialog options");
            QStringList properties = kind == "open"
                ? QStringList { "openFile", "openDirectory", "multiSelections", "showHiddenFiles",
                      "createDirectory", "noResolveAliases", "treatPackageAsDirectory" }
                : QStringList { "showHiddenFiles", "createDirectory", "treatPackageAsDirectory" };
            for (auto value : options.value("properties").toArray())
                if (!value.isString() || !properties.contains(value.toString()))
                    return failure(
                        "ERR_FIREFLY_UNSUPPORTED_API", "File dialog property is unsupported");
            if (kind == "open") {
                auto selected = options.value("properties").toArray();
                if (!selected.contains("openFile") && !selected.contains("openDirectory"))
                    return failure(
                        "ERR_INVALID_ARG_VALUE", "Open dialog must select files or directories");
            }

            for (auto value : options.value("filters").toArray()) {
                if (!value.isObject())
                    return failure("ERR_INVALID_ARG_VALUE", "Invalid file filter");
                auto item = value.toObject();
                if (item.size() != 2 || !item.value("name").isString()
                    || !text(item.value("name").toString()) || !item.value("extensions").isArray()
                    || item.value("extensions").toArray().isEmpty())
                    return failure("ERR_INVALID_ARG_VALUE", "Invalid file filter");
                for (auto extension : item.value("extensions").toArray()) {
                    auto value = extension.toString();
                    if (!extension.isString() || !text(value) || value.isEmpty()
                        || value.toUtf8().size() > 256
                        || (value != "*"
                            && (value.contains('.') || value.contains('*') || value.contains('/')
                                || value.contains('\\'))))
                        return failure("ERR_INVALID_ARG_VALUE", "Invalid filter extension");
                }
            }
        }

        return {};
    }

    QString label(QString value, bool normalize)
    {
        // Qt uses ampersands for access keys. With normalization disabled, preserve
        // literal user labels. On Cocoa normalized access-key markers are removed.
        if (!normalize)
            return value.replace("&", "&&");
#ifdef Q_OS_MACOS
        QString result;
        for (qsizetype index = 0; index < value.size(); ++index) {
            if (value[index] == '&') {
                if (index + 1 < value.size() && value[index + 1] == '&') {
                    result += '&';
                    ++index;
                }
            } else
                result += value[index];
        }

        return result.replace("&", "&&");
#else
        return value;
#endif
    }

    struct MessageState {
        QPointer<QMessageBox> box;
        QPointer<QCheckBox> checkbox;
        QList<QAbstractButton*> buttons;
        Completion complete;
        int cancel {};
        int selected { -1 };
        bool checked {};
        bool finished {};
    };

    void finish_message(std::shared_ptr<MessageState> const& state, int response)
    {
        if (state->finished)
            return;
        state->finished = true;
        bool checked = state->checkbox ? state->checkbox->isChecked() : state->checked;
        auto complete = std::move(state->complete);
        if (state->box)
            state->box->deleteLater();
        if (complete)
            complete({ QJsonObject { { "response", response }, { "checkboxChecked", checked } }, {},
                {} });
    }

    class MessageOperation final : public Detail::Operation {
    public:
        explicit MessageOperation(std::shared_ptr<MessageState> state)
            : state(std::move(state))
        {
        }

        ~MessageOperation() override
        {
            cancel();
        }

        void cancel() override
        {
            if (state->finished)
                return;
            if (state->box)
                state->box->reject();
            else
                finish_message(state, state->cancel);
        }

    private:
        std::shared_ptr<MessageState> state;
    };

    std::shared_ptr<MessageState> message_box(
        QJsonObject const& options, QWidget* parent, Completion complete)
    {
        auto state = std::make_shared<MessageState>();
        state->cancel = options.value("cancelId").toInt();
        state->checked = options.value("checkboxChecked").toBool();
        state->complete = std::move(complete);
        auto* box = new QMessageBox(parent);
        state->box = box;
        box->setTextFormat(Qt::PlainText);
        box->setWindowTitle(options.value("title").toString());
        box->setText(options.value("message").toString());
        box->setInformativeText(options.value("detail").toString());
        auto type = options.value("type").toString();
        box->setIcon(type == "info"  ? QMessageBox::Information
                : type == "error"    ? QMessageBox::Critical
                : type == "warning"  ? QMessageBox::Warning
                : type == "question" ? QMessageBox::Question
                                     : QMessageBox::NoIcon);
        auto buttons = options.value("buttons").toArray();
        for (qsizetype index = 0; index < buttons.size(); ++index) {
            auto* button = box->addButton(
                label(buttons[index].toString(), options.value("normalizeAccessKeys").toBool()),
                index == state->cancel ? QMessageBox::RejectRole : QMessageBox::ActionRole);
            state->buttons.append(button);
            if (options.value("defaultId").toInt(-1) < 0)
                button->setAutoDefault(false);
        }

        auto default_id = options.value("defaultId").toInt(-1);
        if (default_id >= 0)
            box->setDefaultButton(qobject_cast<QPushButton*>(state->buttons[default_id]));
        box->setEscapeButton(state->buttons[state->cancel]);
        auto checkbox = options.value("checkboxLabel").toString();
        if (!checkbox.isEmpty()) {
            auto* check = new QCheckBox(label(checkbox, false), box);
            check->setChecked(state->checked);
            state->checkbox = check;
            QObject::connect(
                check, &QCheckBox::toggled, box, [weak = std::weak_ptr(state)](bool checked) {
                    if (auto current = weak.lock())
                        current->checked = checked;
                });
            box->setCheckBox(check);
        }

        std::weak_ptr<MessageState> weak = state;
        QObject::connect(box, &QMessageBox::buttonClicked, box, [weak](QAbstractButton* button) {
            if (auto value = weak.lock()) {
                auto index = value->buttons.indexOf(button);
                if (index >= 0)
                    value->selected = static_cast<int>(index);
            }
        });
        QObject::connect(box, &QMessageBox::finished, box, [weak](int) {
            auto value = weak.lock();
            if (!value)
                return;
            // buttonClicked precedes done()/finished in Qt; retain its original
            // application index while native helpers finish closing the dialog.
            // Never settle on buttonClicked: native closure still owns admission.
            auto response = value->selected;
            if (response < 0 && value->box)
                response = static_cast<int>(value->buttons.indexOf(value->box->clickedButton()));
            finish_message(value, response < 0 ? value->cancel : response);
        });
        QObject::connect(box, &QObject::destroyed, [weak] {
            if (auto value = weak.lock())
                finish_message(value, value->cancel);
        });
        return state;
    }

    bool real_ui()
    {
        return qobject_cast<QApplication*>(QCoreApplication::instance())
            && QStringList { "cocoa", "xcb" }.contains(QGuiApplication::platformName());
    }

    Detail::Started start_message(QJsonObject const& options, QWidget* parent, Completion complete)
    {
        auto state = message_box(options, parent, std::move(complete));
        auto operation = std::make_shared<MessageOperation>(state);
        state->box->open();
        return { operation, {} };
    }

    Result sync_message(QJsonObject const& options, QWidget* parent)
    {
        Result result;
        auto state = message_box(options, parent, [&](Result value) { result = std::move(value); });
        state->box->exec();
        if (!state->finished)
            finish_message(state, state->cancel);
        return result;
    }

    Detail::Started start_native(
        int, QString const& kind, QJsonObject const& options, QWidget* parent, Completion complete)
    {
        if (!real_ui())
            return { {},
                failure("ERR_FIREFLY_UNSUPPORTED_API",
                    "Native dialogs require a verified desktop UI backend") };
        if (kind != "message")
            return Detail::start_files(kind, options, parent, std::move(complete));
        return start_message(options, parent, std::move(complete));
    }

    Result sync_native(QJsonObject const& options, QWidget* parent)
    {
        if (!real_ui())
            return failure("ERR_FIREFLY_UNSUPPORTED_API",
                "Native dialogs require a verified desktop UI backend");
        return sync_message(options, parent);
    }

    std::atomic<unsigned> pending_native {};

    struct Ticket {
        bool released {};

        void release()
        {
            if (!released) {
                released = true;
                pending_native.fetch_sub(1);
            }
        }
    };

    std::shared_ptr<Ticket> admit()
    {
        auto value = pending_native.load();
        while (value < 16)
            if (pending_native.compare_exchange_weak(value, value + 1))
                return std::make_shared<Ticket>();
        return {};
    }

    struct State;

    struct Record {
        std::weak_ptr<State> owner;
        int id;
        std::shared_ptr<Ticket> ticket;
        Completion complete;
        std::shared_ptr<Detail::Operation> operation;
        QMetaObject::Connection parent_connection;
        std::optional<Result> early;
        bool admitting { true }, finished {}, cancel_requested {};
    };

    struct State {
        std::unordered_map<int, std::shared_ptr<Record>> pending;
        int last {};
        bool stopped {};
    };

    void finish(std::shared_ptr<Record> const& record, Result result)
    {
        if (record->finished)
            return;
        if (record->admitting) {
            if (!record->early)
                record->early = std::move(result);
            return;
        }

        record->finished = true;
        record->ticket->release();
        QObject::disconnect(record->parent_connection);
        if (auto owner = record->owner.lock())
            owner->pending.erase(record->id);
        auto complete = std::move(record->complete);
        auto operation = std::move(record->operation);
        if (complete)
            complete(std::move(result));
    }

    using Async = std::function<Detail::Started(
        int, QString const&, QJsonObject const&, QWidget*, Completion)>;
    using Sync = std::function<Result(QJsonObject const&, QWidget*)>;

    class NativeProvider final : public Provider {
    public:
        NativeProvider(Async async, Sync sync)
            : state(std::make_shared<State>())
            , async(std::move(async))
            , sync(std::move(sync))
        {
        }

        ~NativeProvider() override
        {
            stop();
        }

        Result start(int id, QString const& kind, QJsonObject const& options, QWidget* parent,
            Completion complete) override
        {
            auto owner = state;
            auto driver = async;
            if (owner->stopped)
                return failure("ERR_FIREFLY_APP_EXITED", "Native dialogs stopped");
            if (!owner_thread())
                return failure("ERR_FIREFLY_DIALOG_NATIVE", "Dialogs require native owner thread");
            if (id <= owner->last || id <= 0 || !complete)
                return failure("ERR_INVALID_ARG_VALUE", "Invalid dialog identity/completion");
            auto valid = validate(kind, options);
            if (!valid.code.empty())
                return valid;
            owner->last = id;
            auto ticket = admit();
            if (!ticket)
                return failure("ERR_FIREFLY_DIALOG_LIMIT", "Too many pending native dialogs");
            auto record = std::make_shared<Record>(owner, id, ticket, std::move(complete));
            owner->pending.emplace(id, record);
            if (parent) {
                std::weak_ptr<Record> weak = record;
                record->parent_connection = QObject::connect(
                    parent, &QObject::destroyed, QCoreApplication::instance(), [weak] {
                        if (auto record = weak.lock()) {
                            record->cancel_requested = true;
                            if (record->operation)
                                record->operation->cancel();
                        }
                    });
            }

            auto started = driver(id, kind, options, parent,
                [record](Result result) { finish(record, std::move(result)); });
            record->operation = std::move(started.operation);
            record->admitting = false;
            if (owner->stopped && started.result.code.empty())
                started.result
                    = failure("ERR_FIREFLY_APP_EXITED", "Dialogs stopped during admission");
            if (!record->operation && started.result.code.empty())
                started.result
                    = failure("ERR_FIREFLY_DIALOG_NATIVE", "Native dialog was not created");
            if (!started.result.code.empty())
                record->complete = {};
            if (record->early) {
                auto value = std::move(*record->early);
                record->early.reset();
                finish(record, std::move(value));
            } else if (!record->operation)
                finish(record, {});
            if ((owner->stopped || record->cancel_requested || !started.result.code.empty())
                && record->operation)
                record->operation->cancel();
            return started.result;
        }

        Result message_sync(QJsonObject const& options, QWidget* parent) override
        {
            auto owner = state;
            auto driver = sync;
            if (owner->stopped)
                return failure("ERR_FIREFLY_APP_EXITED", "Native dialogs stopped");
            if (!owner_thread())
                return failure("ERR_FIREFLY_DIALOG_NATIVE", "Dialogs require native owner thread");
            auto valid = validate("message", options);
            if (!valid.code.empty())
                return valid;
            auto ticket = admit();
            if (!ticket)
                return failure("ERR_FIREFLY_DIALOG_LIMIT", "Too many pending native dialogs");
            auto result = driver(options, parent);
            ticket->release();
            return result;
        }

        void cancel(int id) override
        {
            auto found = state->pending.find(id);
            if (found == state->pending.end())
                return;
            auto record = found->second;
            record->cancel_requested = true;
            if (record->operation)
                record->operation->cancel();
        }

        void stop() override
        {
            state->stopped = true;
            auto records = state->pending;
            for (auto const& [id, record] : records) {
                (void)id;
                record->complete = {};
                record->cancel_requested = true;
                if (record->operation)
                    record->operation->cancel();
            }
        }

    private:
        std::shared_ptr<State> state;
        Async async;
        Sync sync;
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<NativeProvider>(start_native, sync_native);
}
#if defined(FIREFLY_DIALOG_PROVIDER_TESTING)
Detail::Started Detail::start_message_for_testing(
    QJsonObject const& options, QWidget* parent, Completion complete)
{
    if (!owner_thread() || !qobject_cast<QApplication*>(QCoreApplication::instance())
        || QGuiApplication::platformName() != "offscreen")
        return { {},
            failure("ERR_FIREFLY_UNSUPPORTED_API",
                "Controlled message widget tests require offscreen Qt") };
    auto valid = validate("message", options);
    if (!valid.code.empty())
        return { {}, valid };
    return start_message(options, parent, std::move(complete));
}

Result Detail::message_sync_for_testing(QJsonObject const& options, QWidget* parent)
{
    if (!owner_thread() || !qobject_cast<QApplication*>(QCoreApplication::instance())
        || QGuiApplication::platformName() != "offscreen")
        return failure(
            "ERR_FIREFLY_UNSUPPORTED_API", "Controlled message widget tests require offscreen Qt");
    auto valid = validate("message", options);
    if (!valid.code.empty())
        return valid;
    return sync_message(options, parent);
}

std::unique_ptr<Provider> Detail::create_provider_for_testing(AsyncDriver async, SyncDriver sync)
{
    return std::make_unique<NativeProvider>(std::move(async), std::move(sync));
}
#endif
}
