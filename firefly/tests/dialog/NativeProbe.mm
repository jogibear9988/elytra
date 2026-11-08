/** Opt-in, one-batch genuine owned UI acceptance. Never part of routine/headless CI. */
#include "native/dialog/Provider.h"
#import <AppKit/AppKit.h>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#import <objc/runtime.h>
#include <optional>
#include <thread>

using Firefly::Dialogs::Provider;
using Firefly::Dialogs::Result;

namespace {
NSSavePanel* current_panel;
NSAlert* current_alert;
using Factory = id (*)(id, SEL);
IMP original_open, original_save, original_alert;
typedef id (*AlertInit)(id __attribute__((ns_consumed)), SEL) __attribute__((ns_returns_retained));
id capture_alert(id receiver __attribute__((ns_consumed)), SEL selector)
    __attribute__((ns_returns_retained));

id capture_alert(id receiver __attribute__((ns_consumed)), SEL selector)
{
    id result = reinterpret_cast<AlertInit>(original_alert)(receiver, selector);
    current_alert = result;
    return result;
}

id capture_open(id type, SEL selector)
{
    id panel = reinterpret_cast<Factory>(original_open)(type, selector);
    current_panel = panel;
    return panel;
}

id capture_save(id type, SEL selector)
{
    id panel = reinterpret_cast<Factory>(original_save)(type, selector);
    current_panel = panel;
    return panel;
}

class Capture {
public:
    Capture()
    {
        open = class_getClassMethod(NSOpenPanel.class, @selector(openPanel));
        save = class_getClassMethod(NSSavePanel.class, @selector(savePanel));
        original_open = method_setImplementation(open, reinterpret_cast<IMP>(capture_open));
        original_save = method_setImplementation(save, reinterpret_cast<IMP>(capture_save));
        auto inherited = class_getInstanceMethod(NSAlert.class, @selector(init));
        original_alert = method_getImplementation(inherited);
        // Add a class-local override if init is inherited: never swizzle NSObject
        // or another superclass. The actual original NSAlert initializer is called.
        class_addMethod(NSAlert.class, @selector(init), reinterpret_cast<IMP>(capture_alert),
            method_getTypeEncoding(inherited));
        alert = class_getInstanceMethod(NSAlert.class, @selector(init));
        method_setImplementation(alert, reinterpret_cast<IMP>(capture_alert));
    }

    ~Capture()
    {
        // Only our process's actual captured panel. Original factories are always
        // called and genuine AppKit callbacks alone settle provider promises.
        if (current_panel)
            [current_panel cancel:nil];
        current_panel = nil;
        current_alert = nil;
        method_setImplementation(alert, original_alert);
        method_setImplementation(save, original_save);
        method_setImplementation(open, original_open);
    }

private:
    Method open, save, alert;
};

class Deadline {
public:
    Deadline()
        : worker([this] {
            std::unique_lock lock(mutex);
            if (!ready.wait_for(lock, std::chrono::seconds(30), [this] { return done; })) {
                fprintf(stderr, "Owned native dialog batch exceeded hard deadline\n");
                std::_Exit(1);
            }
        })
    {
    }

    ~Deadline()
    {
        {
            std::lock_guard lock(mutex);
            done = true;
        }

        ready.notify_all();
        worker.join();
    }

private:
    std::mutex mutex;
    std::condition_variable ready;
    bool done {};
    std::thread worker;
};

bool passed = true;

void check(bool value, char const* message)
{
    if (!value) {
        fprintf(stderr, "FAIL actual native dialog batch: %s\n", message);
        passed = false;
    }
}

QJsonObject message_options(QString title, bool checked)
{
    return { { "type", "warning" }, { "title", title }, { "message", "<b>Plain owned message</b>" },
        { "detail", "Visible detail Grüße" },
        { "buttons", QJsonArray { "Discard", "Keep", "Cancel" } }, { "defaultId", 1 },
        { "cancelId", 2 }, { "checkboxLabel", "Remember owned test" },
        { "checkboxChecked", checked }, { "normalizeAccessKeys", true }, { "noLink", true } };
}

QMessageBox* owned_message(QWidget* parent)
{
    for (auto* widget : QApplication::topLevelWidgets())
        if (auto* box = qobject_cast<QMessageBox*>(widget))
            // QMessageBox deliberately ignores window titles on macOS. Identify only
            // this batch's owned parent and actual message, never a system window title.
            if (box->parentWidget() == parent && box->text() == "<b>Plain owned message</b>"
                && box->isVisible())
                return box;
    return nullptr;
}

bool click_message(QMessageBox& box, QString const& button, bool checked)
{
    check(box.textFormat() == Qt::PlainText && box.text() == "<b>Plain owned message</b>"
            && box.informativeText() == "Visible detail Grüße",
        "real message/detail are visible plain text");
    check(box.defaultButton() && box.defaultButton()->text() == "Keep",
        "native default button maps original index");
    if (box.testAttribute(Qt::WA_DontShowOnScreen)) {
        // Qt's Cocoa helper displays NSAlert, not its backing QWidget controls.
        // Changing the backing QCheckBox does not change the visible suppression
        // button, and hiding the native helper restores its real checkbox state.
        if (!current_alert || !current_alert.window.visible)
            return false;
        check([current_alert.messageText isEqualToString:@"<b>Plain owned message</b>"] &&
                [current_alert.informativeText isEqualToString:@"Visible detail Grüße"],
            "actual owned NSAlert text matches Qt dialog");
        check(current_alert.showsSuppressionButton, "actual NSAlert checkbox exists");
        current_alert.suppressionButton.state
            = checked ? NSControlStateValueOn : NSControlStateValueOff;
        for (NSButton* candidate in current_alert.buttons) {
            if ([candidate.title
                    isEqualToString:[[NSString alloc]
                                        initWithCharacters:reinterpret_cast<unichar const*>(
                                                               button.utf16())
                                                    length:static_cast<NSUInteger>(
                                                               button.size())]]) {
                [candidate performClick:nil];
                return true;
            }
        }

        check(false, "owned actual NSAlert button missing");
        return false;
    }

    auto* checkbox = box.checkBox();
    check(checkbox != nullptr, "real Qt checkbox exists");
    if (checkbox)
        checkbox->setChecked(checked);
    for (auto* candidate : box.buttons())
        if (candidate->text() == button) {
            candidate->click();
            return true;
        }

    check(false, "owned message action button missing");
    box.reject();
    return true;
}

void diagnostic(char const* phase, Result const& result)
{
    fprintf(stderr, "%s actual result code=%s message=%s value=%s\n", phase, result.code.c_str(),
        result.message.c_str(),
        QJsonDocument(result.value).toJson(QJsonDocument::Compact).constData());
}

struct Waiting {
    QPointer<QEventLoop> loop;
    std::optional<Result> result;
};

Result asynchronous(Provider& provider, int id, QString const& kind, QJsonObject const& options,
    QWidget& parent, std::function<bool()> action)
{
    current_panel = nil;
    current_alert = nil;
    auto waiting = std::make_shared<Waiting>();
    QEventLoop loop;
    waiting->loop = &loop;
    auto admitted = provider.start(id, kind, options, &parent, [waiting](Result result) {
        waiting->result = std::move(result);
        if (waiting->loop)
            waiting->loop->quit();
    });
    if (!admitted.code.empty())
        return admitted;
    QTimer poll;
    poll.setInterval(25);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (action())
            poll.stop();
    });
    poll.start();
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        check(false, "actual native dialog phase deadline");
        provider.cancel(id);
        loop.quit();
    });
    timeout.start();
    if (!waiting->result)
        loop.exec();
    if (!waiting->result)
        return { {}, "ERR_NATIVE_PROBE_TIMEOUT", "Native panel completion missing" };
    return std::move(*waiting->result);
}

int run()
{
    if (QGuiApplication::platformName() != "cocoa") {
        fprintf(stderr, "Native dialog UI batch requires actual Cocoa backend\n");
        return 1;
    }

    Deadline deadline;
    Capture capture;
    QTemporaryDir fixture(QDir::tempPath() + "/elytra-dialog-ui-XXXXXX");
    if (!fixture.isValid())
        return 1;
    auto provider = Provider::create();
    QWidget parent;
    parent.setWindowTitle("Elytra owned native dialog batch");
    parent.resize(360, 80);
    parent.show();
    auto async = asynchronous(
        *provider, 1, "message", message_options("Elytra owned async message", false), parent, [&] {
            auto* box = owned_message(&parent);
            if (!box)
                return false;
            return click_message(*box, "Keep", true);
        });
    if (!async.code.empty() || async.value.value("response").toInt(-1) != 1
        || !async.value.value("checkboxChecked").toBool())
        diagnostic("async message", async);
    check(async.code.empty() && async.value.value("response").toInt(-1) == 1
            && async.value.value("checkboxChecked").toBool(),
        "actual async message original index/checkbox completion");
    if (!passed) {
        provider->stop();
        return 1;
    }

    current_alert = nil;
    bool sync_acted = false;
    QTimer sync_action;
    sync_action.setInterval(25);
    QObject::connect(&sync_action, &QTimer::timeout, &parent, [&] {
        auto* box = owned_message(&parent);
        if (box) {
            if (click_message(*box, "Cancel", false)) {
                sync_acted = true;
                sync_action.stop();
            }
        }
    });
    sync_action.start();
    QTimer sync_timeout;
    sync_timeout.setSingleShot(true);
    sync_timeout.setInterval(5000);
    QObject::connect(&sync_timeout, &QTimer::timeout, &parent, [&] {
        check(false, "actual sync message deadline");
        if (auto* box = owned_message(&parent))
            box->reject();
    });
    sync_timeout.start();
    auto sync = provider->message_sync(message_options("Elytra owned sync message", true), &parent);
    sync_timeout.stop();
    sync_action.stop();
    if (!sync_acted || !sync.code.empty() || sync.value.value("response").toInt(-1) != 2
        || sync.value.value("checkboxChecked").toBool())
        diagnostic("sync message", sync);
    check(sync_acted && sync.code.empty() && sync.value.value("response").toInt(-1) == 2
            && !sync.value.value("checkboxChecked").toBool(),
        "actual synchronous message result and checkbox");
    if (!passed) {
        provider->stop();
        return 1;
    }

    QJsonObject open { { "title", "Elytra owned open cancellation" },
        { "defaultPath", fixture.path() }, { "buttonLabel", "Select owned test" },
        { "message", "Owned test only" },
        { "filters",
            QJsonArray { QJsonObject { { "name", "Text" }, { "extensions", QJsonArray { "txt" } } },
                QJsonObject { { "name", "All" }, { "extensions", QJsonArray { "*" } } } } },
        { "properties",
            QJsonArray { "openFile", "openDirectory", "multiSelections", "showHiddenFiles",
                "createDirectory", "noResolveAliases", "treatPackageAsDirectory" } } };
    auto opened = asynchronous(*provider, 2, "open", open, parent, [&] {
        NSOpenPanel* panel =
            [current_panel isKindOfClass:NSOpenPanel.class] ? (NSOpenPanel*)current_panel : nil;
        if (!panel)
            return false;
        check(panel.canChooseFiles && panel.canChooseDirectories && panel.allowsMultipleSelection
                && panel.showsHiddenFiles && panel.treatsFilePackagesAsDirectories
                && !panel.resolvesAliases && panel.accessoryView,
            "actual AppKit mixed/multiple/package/alias/filter configuration");
        [panel cancel:nil];
        return true;
    });
    if (!opened.code.empty() || !opened.value.value("canceled").toBool())
        diagnostic("open cancellation", opened);
    check(opened.code.empty() && opened.value.value("canceled").toBool()
            && opened.value.value("filePaths").toArray().isEmpty(),
        "actual AppKit open cancellation callback");
    if (!passed) {
        provider->stop();
        return 1;
    }

    auto filename = QString::fromUtf8("Saved Grüße-𝄞.txt");
    auto target = fixture.filePath(filename);
    QJsonObject save { { "title", "Elytra owned save path" }, { "defaultPath", target },
        { "buttonLabel", "Choose owned path" }, { "message", "No file will be written" },
        { "nameFieldLabel", "Owned filename:" }, { "showsTagField", false },
        { "filters",
            QJsonArray {
                QJsonObject { { "name", "Text" }, { "extensions", QJsonArray { "txt" } } } } },
        { "properties", QJsonArray { "createDirectory", "treatPackageAsDirectory" } } };
    auto saved = asynchronous(*provider, 3, "save", save, parent, [&] {
        if (!current_panel || [current_panel isKindOfClass:NSOpenPanel.class])
            return false;
        check(!current_panel.showsTagField &&
                [current_panel.nameFieldLabel isEqualToString:@"Owned filename:"],
            "actual save labels/tag field configured");
        [current_panel ok:nil];
        return true;
    });
    if (!saved.code.empty() || saved.value.value("canceled").toBool(true))
        diagnostic("save selection", saved);
    check(saved.code.empty() && !saved.value.value("canceled").toBool(true)
            && saved.value.value("filePath").toString().normalized(QString::NormalizationForm_C)
                == target.normalized(QString::NormalizationForm_C)
            && !QFileInfo::exists(target),
        "actual native save returns owned nonexistent Unicode path without writing file");
    provider->stop();
    parent.close();
    current_panel = nil;
    if (!passed)
        return 1;
    puts("PASS one owned actual Cocoa UI batch: Qt async/sync message index+checkbox, real "
         "open-panel configuration/cancel, real nonexistent Unicode save path; no open-file "
         "selection/multiselect result acceptance claim");
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc != 2 || QString::fromUtf8(argv[1]) != "--ui-batch") {
        fprintf(stderr, "Explicit --ui-batch required; never run in routine CI\n");
        return 2;
    }

    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    @autoreleasepool {
        @try {
            return run();
        } @catch (NSException* exception) {
            fprintf(
                stderr, "Actual owned dialog UI failed: %s\n", exception.description.UTF8String);
            return 1;
        }
    }
}
