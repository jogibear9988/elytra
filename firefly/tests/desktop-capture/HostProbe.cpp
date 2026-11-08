/** Production dispatcher with a controlled native peer: no OS enumeration, capture or TCC. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/desktop-capture/Host.h"
#include "native/desktop-capture/Provider.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <QCoreApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using Firefly::DesktopCapture::Completion;
using Firefly::DesktopCapture::Result;
std::vector<Completion> completions;
bool passed = true;

void check(bool condition, char const* description)
{
    if (!condition) {
        fprintf(stderr, "FAIL desktop capture host: %s\n", description);
        passed = false;
    }
}

void complete(size_t index, Result result)
{
    // Only the controlled provider's completion runs on this worker. The actual
    // production host must transfer plain data through Qt before notifying owner.
    auto callback = completions.at(index);
    std::thread worker([callback = std::move(callback), result = std::move(result)]() mutable {
        callback(std::move(result));
    });
    worker.join();
}

void pump()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}

Result source()
{
    return { { { "screen:42:0", "Controlled native peer", "42" } }, {}, {} };
}
}

// This symbol replaces only the OS provider at link time. Host.cpp is compiled
// unchanged, so request validation, thread marshaling and cancellation are real.
namespace Firefly::DesktopCapture {
void enumerate(Completion completion)
{
    completions.push_back(std::move(completion));
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::shared_ptr<Firefly::DesktopCapture::Controller> controller;
    std::shared_ptr<Firefly::DesktopCapture::Controller> detached_controller;
    std::vector<std::string> delivered;
    size_t detached_notifications = 0;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root host { JS::Object::create(realm, nullptr) };
        controller = Firefly::DesktopCapture::install_host(
            realm, *host, *receiver, [&](std::string payload) {
                check(
                    QThread::currentThread() == application.thread(), "notify on VM owner thread");
                delivered.push_back(std::move(payload));
            });
        realm.global_object().define_direct_property(
            "captureHost"_utf16, host.ptr(), JS::default_attributes);
        auto valid = [&](char const* source) -> JS::ThrowCompletionOr<void> {
            auto result = TRY(modules.run_eval(source));
            check(result.is_boolean() && result.as_bool(), source);
            return {};
        };

        TRY(valid("(() => { for (const id of [0,-1,NaN,Infinity,1.5,'1',2147483648]) { "
                  "try { captureHost.desktopCaptureStart(id); return false; } catch(e) { "
                  "if(e.code!=='ERR_INVALID_ARG_TYPE')return false; } } return true; })()"));
        check(completions.empty(), "invalid requests never reach provider");
        TRY(modules.run_eval("captureHost.desktopCaptureStart(1)"));
        complete(0, source());
        check(delivered.empty(), "worker completion cannot notify before owner dispatch");
        pump();
        check(delivered.size() == 1, "first completion delivered once");
        if (!delivered.empty()) {
            auto document = QJsonDocument::fromJson(QByteArray::fromStdString(delivered[0]));
            check(document.isObject() && document.object()["id"].toInt() == 1,
                "completion JSON retains request identity");
            auto sources = document.object()["sources"].toArray();
            check(sources.size() == 1 && sources[0].toObject()["display_id"] == "42",
                "native source metadata marshaled intact");
        }

        complete(0, source());
        pump();
        check(delivered.size() == 1, "duplicate native completion dropped");
        TRY(modules.run_eval(
            "captureHost.desktopCaptureStart(2); captureHost.desktopCaptureCancel(2)"));
        complete(1, source());
        pump();
        check(delivered.size() == 1, "canceled late completion dropped");
        TRY(valid(
            "(() => { for(const id of [1,2]) { try { captureHost.desktopCaptureStart(id); "
            "return false; } catch(e) { if(e.code!=='ERR_FIREFLY_CAPTURE_DATA')return false; } } "
            "return true; })()"));
        TRY(modules.run_eval("for(let id=3;id<=10;id++)captureHost.desktopCaptureStart(id)"));
        check(completions.size() == 10, "eight active requests admitted");
        TRY(valid("(() => { try { captureHost.desktopCaptureStart(11); return false; } "
                  "catch(e) { return e.code==='ERR_FIREFLY_CAPTURE_LIMIT'; } })()"));
        check(completions.size() == 10, "quota refusal never reaches provider");
        TRY(modules.run_eval("for(let id=3;id<=10;id++)captureHost.desktopCaptureCancel(id)"));
        for (size_t index = 2; index < 10; ++index)
            complete(index, source());
        pump();
        check(delivered.size() == 1, "canceled quota batch cannot notify");
        TRY(modules.run_eval("captureHost.desktopCaptureStart(11)"));
        complete(10, { {}, "TEST_PROVIDER", "Controlled provider failure" });
        pump();
        check(delivered.size() == 2, "error completion marshaled");
        if (delivered.size() == 2) {
            auto payload
                = QJsonDocument::fromJson(QByteArray::fromStdString(delivered[1])).object();
            check(payload["code"] == "TEST_PROVIDER" && payload["id"].toInt() == 11,
                "provider failure is not successful empty enumeration");
        }

        TRY(modules.run_eval("captureHost.desktopCaptureStart(12)"));
        complete(11, source());
        controller->stop();
        pump();
        check(delivered.size() == 2, "stop retires already queued completion");
        complete(11, source());
        pump();
        check(delivered.size() == 2, "stop retires subsequent worker completion");
        TRY(valid("(() => { try { captureHost.desktopCaptureStart(13); return false; } "
                  "catch(e) { return e.code==='ERR_FIREFLY_APP_EXITED'; } })()"));

        auto temporary_receiver = std::make_unique<QObject>();
        GC::Root temporary_host { JS::Object::create(realm, nullptr) };
        detached_controller = Firefly::DesktopCapture::install_host(realm, *temporary_host,
            *temporary_receiver, [&](std::string) { ++detached_notifications; });
        realm.global_object().define_direct_property(
            "temporaryCaptureHost"_utf16, temporary_host.ptr(), JS::default_attributes);
        TRY(modules.run_eval("temporaryCaptureHost.desktopCaptureStart(1)"));
        complete(12, source());
        // Production teardown orders stop before owner deletion. Deleting a Qt
        // receiver must also retire queued events, without executing a VM callback.
        detached_controller->stop();
        temporary_receiver.reset();
        pump();
        complete(12, source());
        pump();
        check(detached_notifications == 0, "receiver teardown drops queued and late completions");
        return {};
    };

    hooks.shutdown = [&] {
        if (controller)
            controller->stop();
        if (detached_controller)
            detached_controller->stop();
    };

    char executable[] = "firefly-desktop-capture-host-probe";
    char option[] = "-e";
    char empty[] = "";
    char* arguments[] = { executable, option, empty, nullptr };
    auto status = Mite::run(3, arguments, hooks);
    controller.reset();
    detached_controller.reset();
    receiver.reset();
    for (size_t index = 0; index < completions.size(); ++index)
        complete(index, source());
    pump();
    check(delivered.size() == 2 && detached_notifications == 0,
        "callbacks after VM/controller teardown have no delivery authority");
    if (status || !passed)
        return 1;
    puts("PASS production desktop capture host worker-to-owner dispatch, data/errors and once-only "
         "delivery");
    puts("PASS production desktop capture host validation, monotonic IDs, quota and cancellation");
    puts("PASS production desktop capture host stop/receiver/VM teardown rejects queued and late "
         "native completions");
    puts("Controlled native provider only: no OS capture, TCC or platform capture acceptance");
    return 0;
}
