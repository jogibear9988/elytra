/** Exact native dispatcher with a controlled provider. No OS authorization or alert delivery. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/notifications/Host.h"
#include "native/notifications/Provider.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>
#include <QCoreApplication>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using Firefly::Notifications::Event;
using Firefly::Notifications::EventCallback;
using Firefly::Notifications::Options;
using Firefly::Notifications::Provider;
bool passed = true;
bool provider_supported = true;
bool provider_available = true;

void check(bool condition, char const* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL notification host: %s\n", message);
        passed = false;
    }
}

class ControlledProvider final : public Provider {
public:
    explicit ControlledProvider(EventCallback callback)
        : callback(std::move(callback))
    {
    }

    void show(uint64_t token, Options options) override
    {
        shown.emplace_back(token, std::move(options));
    }

    void close(uint64_t token) override
    {
        closed.push_back(token);
        // A native removal can synchronously report closure. Authority must be
        // retired first, and no dispatcher mutex may remain locked across this call.
        callback({ token, "close", {} });
    }

    void stop() override
    {
        ++stops;
        callback({ 1, "failed", "Synchronous stop callback" });
    }

    EventCallback callback;
    std::vector<std::pair<uint64_t, Options>> shown;
    std::vector<uint64_t> closed;
    size_t stops {};
};

std::vector<std::shared_ptr<ControlledProvider>> providers;

void worker_event(std::shared_ptr<ControlledProvider> const& provider, Event event)
{
    std::thread worker([callback = provider->callback, event = std::move(event)]() mutable {
        callback(std::move(event));
    });
    worker.join();
}

void pump()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}
}

namespace Firefly::Notifications {
bool Provider::supported()
{
    return provider_supported;
}

std::shared_ptr<Provider> Provider::create(EventCallback callback)
{
    if (!provider_available)
        return {};
    auto provider = std::make_shared<ControlledProvider>(std::move(callback));
    providers.push_back(provider);
    return provider;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::shared_ptr<Firefly::Notifications::Controller> controller;
    std::shared_ptr<Firefly::Notifications::Controller> temporary_controller;
    std::vector<QJsonObject> delivered;
    size_t temporary_notifications = 0;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root host { JS::Object::create(realm, nullptr) };
        controller = Firefly::Notifications::install_host(
            realm, *host, *receiver, [&](std::string payload) {
                check(QThread::currentThread() == application.thread(), "notify stays on VM owner");
                auto document = QJsonDocument::fromJson(QByteArray::fromStdString(payload));
                check(document.isObject(), "notify is an actual JSON object");
                delivered.push_back(document.object());
            });
        realm.global_object().define_direct_property(
            "notificationHost"_utf16, host.ptr(), JS::default_attributes);
        auto valid = [&](char const* source) -> JS::ThrowCompletionOr<void> {
            auto value = TRY(modules.run_eval(source));
            check(value.is_boolean() && value.as_bool(), source);
            return {};
        };

        TRY(valid("notificationHost.notificationSupported()===true"));
        provider_supported = false;
        TRY(valid("notificationHost.notificationSupported()===false"));
        provider_supported = true;
        check(providers.empty(), "support query cannot create a native presenter");
        TRY(valid("(() => { for(const id of [0,-1,NaN,Infinity,1.5,'1',2147483648]) { "
                  "for(const method of ['notificationShow','notificationClose']) { try { "
                  "notificationHost[method](id,'{}');return false; } catch(e) { "
                  "if(e.code!=='ERR_INVALID_ARG_TYPE')return false; } } } return true; })()"));
        TRY(valid(R"JS((() => {
 const good={title:'title',body:'body',silent:false,actions:[]};
 const invalid=[
  [null,'ERR_INVALID_ARG_TYPE'],['no json','ERR_INVALID_ARG_TYPE'],['[]','ERR_INVALID_ARG_TYPE'],
  ['{}','ERR_INVALID_ARG_TYPE'],
  [JSON.stringify({...good,silent:0}),'ERR_INVALID_ARG_TYPE'],
  [JSON.stringify({...good,title:'bad\0text'}),'ERR_INVALID_ARG_TYPE'],
  [JSON.stringify({...good,body:'bad\0text'}),'ERR_INVALID_ARG_TYPE'],
  [JSON.stringify({...good,title:'x'.repeat(4097)}),'ERR_OUT_OF_RANGE'],
  [JSON.stringify({...good,body:'x'.repeat(65537)}),'ERR_OUT_OF_RANGE'],
  [' '.repeat(524289),'ERR_OUT_OF_RANGE'],
  [JSON.stringify({...good,subtitle:'unsupported'}),'ERR_FIREFLY_UNSUPPORTED_API'],
  [JSON.stringify({...good,actions:[0]}),'ERR_INVALID_ARG_TYPE'],
  [JSON.stringify({...good,actions:[{type:'reply',text:'x'}]}),'ERR_FIREFLY_UNSUPPORTED_API'],
  [JSON.stringify({...good,actions:[{type:'button',text:'bad\0text'}]}),'ERR_INVALID_ARG_TYPE'],
  [JSON.stringify({...good,actions:[{type:'button',text:'x'.repeat(1025)}]}),'ERR_OUT_OF_RANGE'],
  [JSON.stringify({...good,actions:[{type:'button',text:'x',id:'unsupported'}]}),'ERR_FIREFLY_UNSUPPORTED_API'],
  [JSON.stringify({...good,actions:Array(5).fill({type:'button',text:'x'})}),'ERR_OUT_OF_RANGE']
 ];
 for(const [snapshot,code] of invalid){try{notificationHost.notificationShow(1,snapshot);return false;}catch(e){if(e.code!==code)return false;}}
 return true;
})())JS"));
        check(providers.empty(), "invalid options never allocate provider");
        TRY(modules.run_eval(R"JS(
const snapshot=JSON.stringify({title:'Grüße 🦋',body:'body',silent:true,actions:[{type:'button',text:'One'},{type:'button',text:'Two'}]});
notificationHost.notificationShow(1,snapshot);
)JS"));
        check(providers.size() == 1, "first valid show allocates provider");
        auto provider = providers.at(0);
        check(provider->shown.size() == 1 && provider->shown[0].first == 1
                && provider->shown[0].second.title == "Grüße 🦋" && provider->shown[0].second.silent
                && provider->shown[0].second.actions.size() == 2,
            "validated UTF-8/options reach genuine host provider boundary");
        worker_event(provider, { 1, "show", {} });
        worker_event(provider, { 1, "show", {} });
        check(delivered.empty(), "worker cannot notify before owner queue runs");
        pump();
        check(delivered.size() == 1 && delivered.back()["type"] == "show",
            "show acknowledgment delivered once");
        worker_event(provider, { 1, "action", {}, 1 });
        worker_event(provider, { 1, "click", {} });
        pump();
        check(delivered.size() == 2 && delivered.back()["type"] == "action"
                && delivered.back()["index"].toInt() == 1,
            "valid action is terminal and duplicate click/close cannot notify");
        TRY(valid("(() => {try{notificationHost.notificationShow(1,snapshot);return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_NOTIFICATION_DATA';}})()"));
        TRY(modules.run_eval("notificationHost.notificationShow(2,snapshot)"));
        worker_event(provider, { 2, "action", {}, 2 });
        pump();
        check(delivered.size() == 3 && delivered.back()["type"] == "failed",
            "action index at action count becomes explicit failed event");
        TRY(modules.run_eval("notificationHost.notificationShow(3,snapshot)"));
        worker_event(provider, { 3, "unexpected", {} });
        pump();
        check(delivered.size() == 4 && delivered.back()["type"] == "failed",
            "unrecognized provider event cannot masquerade as successful delivery");
        TRY(modules.run_eval("notificationHost.notificationShow(4,snapshot)"));
        worker_event(provider, { 4, "failed", std::string(4097, 'x') });
        pump();
        check(delivered.size() == 5
                && delivered.back()["message"] == "Invalid native notification event",
            "native event message bound enforced");
        TRY(modules.run_eval("notificationHost.notificationShow(5,snapshot)"));
        worker_event(provider, { 5, "click", {} });
        pump();
        check(delivered.size() == 6 && delivered.back()["type"] == "click", "native click event");
        TRY(modules.run_eval("notificationHost.notificationShow(6,snapshot)"));
        worker_event(provider, { 6, "close", {} });
        pump();
        check(
            delivered.size() == 7 && delivered.back()["type"] == "close", "native dismissal event");
        TRY(modules.run_eval("notificationHost.notificationShow(7,snapshot)"));
        worker_event(provider, { 7, "failed", "Controlled native failure" });
        pump();
        check(delivered.size() == 8 && delivered.back()["message"] == "Controlled native failure",
            "native failure event retained");
        TRY(modules.run_eval(
            "notificationHost.notificationShow(8,snapshot);"
            "notificationHost.notificationClose(8);notificationHost.notificationClose(8)"));
        worker_event(provider, { 8, "show", {} });
        worker_event(provider, { 8, "click", {} });
        pump();
        check(
            delivered.size() == 8, "explicit/repeated close revokes queued and subsequent events");
        TRY(modules.run_eval(
            "for(let token=9;token<=72;token++)notificationHost.notificationShow(token,snapshot)"));
        auto shows = provider->shown.size();
        TRY(valid("(() => {try{notificationHost.notificationShow(73,snapshot);return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_NOTIFICATION_LIMIT';}})()"));
        check(provider->shown.size() == shows, "quota64 refusal never reaches provider");
        TRY(modules.run_eval(
            "for(let token=9;token<=72;token++)notificationHost.notificationClose(token);"
            "notificationHost.notificationShow(73,snapshot)"));
        worker_event(provider, { 73, "show", {} });
        controller->stop();
        pump();
        worker_event(provider, { 73, "failed", "late" });
        pump();
        check(delivered.size() == 8 && provider->stops == 1,
            "stop and synchronous provider-stop callback retire all delivery authority");
        TRY(valid("notificationHost.notificationSupported()===false"));
        TRY(valid("(() => {try{notificationHost.notificationShow(74,snapshot);return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_APP_EXITED';}})()"));

        auto temporary_receiver = std::make_unique<QObject>();
        GC::Root temporary_host { JS::Object::create(realm, nullptr) };
        temporary_controller = Firefly::Notifications::install_host(realm, *temporary_host,
            *temporary_receiver, [&](std::string) { ++temporary_notifications; });
        realm.global_object().define_direct_property(
            "temporaryNotificationHost"_utf16, temporary_host.ptr(), JS::default_attributes);
        TRY(modules.run_eval("temporaryNotificationHost.notificationShow(1,snapshot)"));
        auto temporary_provider = providers.at(1);
        worker_event(temporary_provider, { 1, "show", {} });
        temporary_controller->stop();
        temporary_receiver.reset();
        pump();
        worker_event(temporary_provider, { 1, "failed", "late" });
        pump();
        check(temporary_notifications == 0,
            "stopped receiver deletion drops queued and late worker events");
        return {};
    };

    hooks.shutdown = [&] {
        if (controller)
            controller->stop();
        if (temporary_controller)
            temporary_controller->stop();
    };

    char executable[] = "firefly-notification-host-probe";
    char flag[] = "-e";
    char empty[] = "";
    char* arguments[] = { executable, flag, empty, nullptr };
    auto status = Mite::run(3, arguments, hooks);
    controller.reset();
    temporary_controller.reset();
    receiver.reset();
    for (auto const& provider : providers) {
        worker_event(provider, { 1, "show", {} });
        worker_event(provider, { 1, "click", {} });
    }

    pump();
    check(delivered.size() == 8 && temporary_notifications == 0,
        "callbacks after native controller/VM teardown cannot notify");
    if (status || !passed)
        return 1;
    puts("PASS production notification host strict tokens/JSON/options/text/actions/quota64");
    puts("PASS production notification host worker-to-owner show/action/click/close/failure "
         "routing and deduplication");
    puts("PASS production notification host close/stop/receiver/VM teardown drops late callbacks");
    puts("Controlled provider only: no OS alert, permission request or platform delivery "
         "acceptance");
    return 0;
}
