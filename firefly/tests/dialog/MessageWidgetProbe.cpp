/** Exact production QMessageBox routines on offscreen widgets, never Cocoa/desktop UI. */
#include "native/dialog/Operation.h"
#include <QApplication>
#include <QCheckBox>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <cstdio>
#include <optional>

namespace Detail = Firefly::Dialogs::Detail;
using Firefly::Dialogs::Result;

namespace {
bool passed = true;

void check(bool value, char const* message)
{
    if (!value) {
        fprintf(stderr, "FAIL exact message widget: %s\n", message);
        passed = false;
    }
}

QJsonObject options(QString const& title)
{
    return { { "type", "warning" }, { "title", title },
        { "message", "<b>Visible plain text</b> " + title }, { "detail", "Visible detail Grüße" },
        { "buttons", QJsonArray { "Discard", "Keep", "Cancel" } }, { "defaultId", 1 },
        { "cancelId", 2 }, { "checkboxLabel", "Remember" }, { "checkboxChecked", false },
        { "normalizeAccessKeys", true }, { "noLink", true } };
}

QMessageBox* find(QString const& title)
{
    for (auto* widget : QApplication::topLevelWidgets())
        if (auto* box = qobject_cast<QMessageBox*>(widget))
            // QMessageBox deliberately ignores window titles on macOS.
            if (box->text() == "<b>Visible plain text</b> " + title)
                return box;
    return nullptr;
}

void diagnostic(Result const& result)
{
    fprintf(stderr, "Result code=%s message=%s value=%s\n", result.code.c_str(),
        result.message.c_str(),
        QJsonDocument(result.value).toJson(QJsonDocument::Compact).constData());
}

void action(QMessageBox* box, QString const& text, bool checked)
{
    check(box && box->isVisible(), "actual offscreen widget is visible to Qt");
    if (!box)
        return;
    check(box->textFormat() == Qt::PlainText && box->text().startsWith("<b>Visible plain text</b> ")
            && box->informativeText() == "Visible detail Grüße",
        "production text/detail remain visible plain-text fields");
    check(box->defaultButton() && box->defaultButton()->text() == "Keep",
        "default original button index is preserved");
    check(box->checkBox() != nullptr, "production checkbox exists");
    if (box->checkBox())
        box->checkBox()->setChecked(checked);
    for (auto* button : box->buttons())
        if (button->text() == text) {
            button->click();
            return;
        }

    check(false, "requested actual Qt button absent");
    box->reject();
}
}

int main(int argc, char** argv)
{
    // Offscreen presentation alone does not disable Cocoa's native theme helpers.
    // This gate exercises Qt widgets, never the system NSAlert path.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    for (int index = 0; index < 3; ++index) {
        auto title = QString("Owned actual message %1").arg(index);
        auto config = options(title);
        std::optional<Result> observed;
        bool button_signal = false;
        unsigned callbacks {};
        QEventLoop loop;
        QPointer<QMessageBox> box;
        auto started = Detail::start_message_for_testing(config, nullptr, [&](Result value) {
            ++callbacks;
            check(button_signal, "completion waits until buttonClicked signal returns");
            check(!box || !box->isVisible(), "completion waits for actual Qt closure");
            observed = std::move(value);
            loop.quit();
        });
        check(
            started.result.code.empty() && started.operation, "actual message operation admitted");
        box = find(title);
        check(box != nullptr, "production message_box created actual QMessageBox");
        if (box)
            QObject::connect(box, &QMessageBox::buttonClicked, &loop,
                [&](QAbstractButton*) { button_signal = true; });
        QTimer::singleShot(0, &loop,
            [&] { action(box, config.value("buttons").toArray()[index].toString(), true); });
        QTimer::singleShot(1000, &loop, [&] {
            if (!observed) {
                check(false, "offscreen message completion deadline");
                started.operation->cancel();
                loop.quit();
            }
        });
        if (!observed)
            loop.exec();
        auto valid = observed && observed->code.empty()
            && observed->value.value("response").toInt(-1) == index
            && observed->value.value("checkboxChecked").toBool();
        check(valid, "each actual Qt button preserves original application index and checkbox");
        if (observed && !valid)
            diagnostic(*observed);
        if (box)
            box->reject();
        check(callbacks == 1, "duplicate native finished cannot settle twice");
        started.operation.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    {
        std::optional<Result> observed;
        auto title = QString("Owned actual cancel");
        auto started = Detail::start_message_for_testing(
            options(title), nullptr, [&](Result result) { observed = std::move(result); });
        auto* box = find(title);
        check(box && box->checkBox(), "cancel fixture actual widget exists");
        if (box && box->checkBox())
            box->checkBox()->setChecked(true);
        started.operation->cancel();
        check(observed && observed->value.value("response").toInt(-1) == 2
                && observed->value.value("checkboxChecked").toBool(),
            "actual reject maps cancelId and current checkbox");
        started.operation.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    {
        std::optional<Result> observed;
        auto parent = std::make_unique<QWidget>();
        auto title = QString("Owned actual parent destruction");
        auto started = Detail::start_message_for_testing(
            options(title), parent.get(), [&](Result result) { observed = std::move(result); });
        auto* box = find(title);
        if (box && box->checkBox())
            box->checkBox()->setChecked(true);
        parent.reset();
        check(observed && observed->value.value("response").toInt(-1) == 2
                && observed->value.value("checkboxChecked").toBool(),
            "actual parent destruction preserves last checkbox snapshot and cancel index");
        started.operation.reset();
    }

    {
        auto title = QString("Owned actual sync");
        bool clicked = false;
        QTimer click;
        click.setInterval(5);
        QObject::connect(&click, &QTimer::timeout, &app, [&] {
            if (auto* box = find(title)) {
                clicked = true;
                action(box, "Keep", true);
                click.stop();
            }
        });
        click.start();
        QTimer deadline;
        deadline.setSingleShot(true);
        deadline.setInterval(1000);
        QObject::connect(&deadline, &QTimer::timeout, &app, [&] {
            check(false, "actual sync widget deadline");
            if (auto* box = find(title))
                box->reject();
        });
        deadline.start();
        auto result = Detail::message_sync_for_testing(options(title), nullptr);
        click.stop();
        deadline.stop();
        auto valid = clicked && result.code.empty() && result.value.value("response").toInt(-1) == 1
            && result.value.value("checkboxChecked").toBool();
        check(valid, "actual exec path preserves original button and checkbox result");
        if (!valid)
            diagnostic(result);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    if (!passed)
        return 1;
    puts("PASS exact production QMessageBox offscreen button "
         "indices/checkbox/finished/cancel/parent/sync semantics; no desktop/Cocoa UI acceptance "
         "claim");
    return 0;
}
