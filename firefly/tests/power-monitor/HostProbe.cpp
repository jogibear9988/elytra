/** Exact power dispatcher with controlled native queries/events; never changes OS power state. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/power-monitor/Host.h"
#include "native/power-monitor/Provider.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>
#include <QCoreApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {
using Firefly::PowerMonitor::Event;
using Firefly::PowerMonitor::EventCallback;
using Firefly::PowerMonitor::Provider;
using Firefly::PowerMonitor::Query;
using PowerResult = Firefly::PowerMonitor::Result;
bool passed = true;
bool fail_query = false;
bool fail_start = false;
bool notify_on_start = false;
std::optional<PowerResult> forced_result;
std::vector<std::string> capability_names { "suspend", "resume", "on-ac", "on-battery",
    "thermal-state-change", "speed-limit-change", "lock-screen", "unlock-screen" };
std::vector<std::pair<Query, int>> queries;
std::vector<EventCallback> callbacks;
size_t starts = 0;
size_t stops = 0;

void check(bool condition, char const* description)
{
    if (!condition) {
        fprintf(stderr, "FAIL power monitor host: %s\n", description);
        passed = false;
    }
}

class ControlledProvider final : public Provider {
public:
    explicit ControlledProvider(EventCallback callback)
        : m_callback(std::move(callback))
    {
    }

    PowerResult start() override
    {
        ++starts;
        if (notify_on_start)
            m_callback({ "resume", {}, -1 });
        if (fail_start)
            return { 0, {}, false, "TEST_POWER_START", "Controlled subscription failure" };
        return {};
    }

    void stop() override
    {
        ++stops;
        // Real native observer removal can race with a final callback.
        m_callback({ "suspend", {}, -1 });
    }

private:
    EventCallback m_callback;
};

void worker_event(EventCallback callback, Event event)
{
    std::thread worker([callback = std::move(callback), event = std::move(event)]() mutable {
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

// Controlled free functions replace only the OS provider. Production Host.cpp
// remains the code under test, including all VM validation and QObject dispatch.
namespace Firefly::PowerMonitor {
PowerResult query(Query kind, int threshold)
{
    queries.emplace_back(kind, threshold);
    if (fail_query)
        return { 0, {}, false, "TEST_POWER_QUERY", "Controlled native failure Grüße" };
    if (forced_result)
        return *forced_result;
    switch (kind) {
    case Query::IdleTime:
        return { 17, {}, false, {}, {} };
    case Query::IdleState:
        return { 0, "idle", false, {}, {} };
    case Query::ThermalState:
        return { 0, "fair", false, {}, {} };
    case Query::BatteryPower:
        return { 0, {}, true, {}, {} };
    case Query::SpeedLimit:
        return { 73, {}, false, {}, {} };
    }

    return { 0, {}, false, "TEST_INVALID_QUERY", "Unexpected controlled query" };
}

std::shared_ptr<Provider> Provider::create(EventCallback callback)
{
    callbacks.push_back(callback);
    return std::make_shared<ControlledProvider>(std::move(callback));
}

std::vector<std::string> Provider::capabilities()
{
    return capability_names;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::vector<std::shared_ptr<Firefly::PowerMonitor::Controller>> controllers;
    std::vector<QJsonObject> delivered;
    size_t temporary_notifications = 0;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        auto install = [&](JS::PropertyKey const& name, QObject& target,
                           std::function<void(std::string)> notify) {
            GC::Root host { JS::Object::create(realm, nullptr) };
            auto controller
                = Firefly::PowerMonitor::install_host(realm, *host, target, std::move(notify));
            realm.global_object().define_direct_property(name, host.ptr(), JS::default_attributes);
            controllers.push_back(controller);
            return controller;
        };

        auto controller = install("powerHost"_utf16, *receiver, [&](std::string payload) {
            check(QThread::currentThread() == application.thread(),
                "notify on native VM owner thread");
            auto document = QJsonDocument::fromJson(QByteArray::fromStdString(payload));
            check(document.isObject(), "native event is bounded JSON object");
            delivered.push_back(document.object());
        });
        auto valid = [&](char const* source) -> JS::ThrowCompletionOr<void> {
            auto value = TRY(modules.run_eval(source));
            check(value.is_boolean() && value.as_bool(), source);
            return {};
        };

        TRY(valid("JSON.parse(powerHost.powerMonitorQuery('idle-time')).value===17"));
        TRY(valid("JSON.parse(powerHost.powerMonitorQuery('idle-state',42)).value==='idle'"));
        check(queries.back().first == Query::IdleState && queries.back().second == 42,
            "validated idle threshold reaches native query");
        TRY(valid("JSON.parse(powerHost.powerMonitorQuery('thermal-state')).value==='fair'"));
        TRY(valid("JSON.parse(powerHost.powerMonitorQuery('battery-power')).value===true"));
        TRY(valid("JSON.parse(powerHost.powerMonitorQuery('speed-limit')).value===73"));
        auto query_count = queries.size();
        TRY(valid(R"JS((() => {
 const cases=[[null,60,'ERR_INVALID_ARG_TYPE'],['unknown',60,'ERR_INVALID_ARG_VALUE'],
 ['idle-state',null,'ERR_INVALID_ARG_TYPE'],['idle-state',1.5,'ERR_INVALID_ARG_TYPE'],
 ['idle-state',NaN,'ERR_INVALID_ARG_TYPE'],['idle-state',Infinity,'ERR_INVALID_ARG_TYPE'],
 ['idle-state',0,'ERR_OUT_OF_RANGE'],['idle-state',-1,'ERR_OUT_OF_RANGE'],
 ['idle-state',2147483648,'ERR_OUT_OF_RANGE']];
 for(const [kind,threshold,code] of cases){try{powerHost.powerMonitorQuery(kind,threshold);return false;}catch(e){if(e.code!==code)return false;}}
 return true;
})())JS"));
        check(queries.size() == query_count, "invalid query arguments cannot reach provider");
        fail_query = true;
        TRY(valid(
            "(() => {try{powerHost.powerMonitorQuery('idle-time');return false;}"
            "catch(e){return e.code==='TEST_POWER_QUERY'&&e.message.includes('Grüße');}})()"));
        fail_query = false;
        for (double invalid : { -1.0, 1.5, std::numeric_limits<double>::infinity() }) {
            forced_result = PowerResult { invalid, {}, false, {}, {} };
            TRY(valid("(() => {try{powerHost.powerMonitorQuery('idle-time');return false;}"
                      "catch(e){return e.code==='ERR_FIREFLY_POWER_MONITOR_DATA';}})()"));
        }

        forced_result = PowerResult { 101, {}, false, {}, {} };
        TRY(valid("(() => {try{powerHost.powerMonitorQuery('speed-limit');return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_POWER_MONITOR_DATA';}})()"));
        forced_result = PowerResult { 0, "invented-state", false, {}, {} };
        TRY(valid("(() => {for(const kind of ['idle-state','thermal-state']){try{"
                  "powerHost.powerMonitorQuery(kind,60);return false;}catch(e){"
                  "if(e.code!=='ERR_FIREFLY_POWER_MONITOR_DATA')return false;}}return true;})()"));
        forced_result.reset();
        TRY(valid("JSON.parse(powerHost.powerMonitorCapabilities()).length===8"));
        auto original_capabilities = capability_names;
        capability_names = { "resume" };
        TRY(valid("JSON.parse(powerHost.powerMonitorCapabilities()).length===8"));
        capability_names = original_capabilities;
        TRY(valid("(() => {for(const id of [0,-1,NaN,Infinity,1.5,'1',2147483648]){"
                  "for(const method of ['powerMonitorStart','powerMonitorStop']){try{"
                  "powerHost[method](id);return false;}catch(e){if(e.code!=='ERR_INVALID_ARG_TYPE')"
                  "return false;}}}return true;})()"));
        check(starts == 0, "invalid epochs cannot create native subscriptions");
        TRY(modules.run_eval("powerHost.powerMonitorStart(1)"));
        auto old_callback = callbacks.back();
        TRY(valid("(() => {try{powerHost.powerMonitorStart(2);return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_POWER_MONITOR_LIMIT';}})()"));
        worker_event(old_callback, { "thermal-state-change", "serious", -1 });
        check(delivered.empty(), "worker completion cannot notify before owner pump");
        pump();
        check(delivered.size() == 1 && delivered.back()["epoch"].toInt() == 1
                && delivered.back()["state"] == "serious",
            "typed thermal frame retains epoch and state");
        worker_event(old_callback, { "speed-limit-change", {}, 73 });
        pump();
        check(delivered.size() == 2 && delivered.back()["limit"].toInt() == 73,
            "typed CPU limit frame retains native value");
        worker_event(old_callback, { "shutdown", {}, -1 });
        pump();
        check(delivered.size() == 2, "known unadvertised native event cannot invent capability");
        worker_event(old_callback, { "unknown-event", "native text", -1 });
        pump();
        check(delivered.size() == 3 && delivered.back()["type"] == "unknown-event",
            "unknown current event reaches public validation instead of fake success");
        worker_event(old_callback, { "resume", std::string(129, 'x'), -1 });
        pump();
        check(delivered.size() == 4 && delivered.back()["type"] == "invalid-native-event",
            "oversized native event is bounded for public explicit refusal");
        TRY(modules.run_eval("powerHost.powerMonitorStop(1)"));
        pump();
        check(delivered.size() == 4, "synchronous stop callback has already lost epoch authority");
        TRY(valid("(() => {try{powerHost.powerMonitorStart(1);return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_POWER_MONITOR_LIMIT';}})()"));
        TRY(modules.run_eval("powerHost.powerMonitorStart(2);powerHost.powerMonitorStop(1)"));
        auto current_callback = callbacks.back();
        worker_event(old_callback, { "resume", {}, -1 });
        worker_event(current_callback, { "resume", {}, -1 });
        pump();
        check(delivered.size() == 5 && delivered.back()["epoch"].toInt() == 2,
            "stale callback/stop cannot retire a newer native epoch");
        TRY(modules.run_eval("powerHost.powerMonitorStop(2)"));
        notify_on_start = true;
        fail_start = true;
        auto before_failure = stops;
        TRY(valid("(() => {try{powerHost.powerMonitorStart(3);return false;}"
                  "catch(e){return e.code==='TEST_POWER_START';}})()"));
        pump();
        check(delivered.size() == 5 && stops > before_failure,
            "failed partial start rolls back and drops synchronous queued callbacks");
        fail_start = false;
        notify_on_start = false;
        TRY(modules.run_eval("powerHost.powerMonitorStart(4)"));
        current_callback = callbacks.back();
        worker_event(current_callback, { "resume", {}, -1 });
        controller->stop();
        pump();
        worker_event(current_callback, { "suspend", {}, -1 });
        pump();
        check(delivered.size() == 5, "controller stop revokes queued and late callback authority");
        TRY(valid("(() => {for(const method of "
                  "['powerMonitorCapabilities','powerMonitorQuery','powerMonitorStart']){"
                  "try{if(method==='powerMonitorStart')powerHost[method](5);else "
                  "powerHost[method]('idle-time');"
                  "return false;}catch(e){if(e.code!=='ERR_FIREFLY_APP_EXITED')return "
                  "false;}}return true;})()"));

        auto bad = install(
            "badPowerHost"_utf16, *receiver, [&](std::string) { ++temporary_notifications; });
        capability_names = { "resume", "resume" };
        TRY(valid("(() => {try{badPowerHost.powerMonitorCapabilities();return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_POWER_MONITOR_DATA';}})()"));
        capability_names = { "Grüße" };
        TRY(valid("(() => {try{badPowerHost.powerMonitorCapabilities();return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_POWER_MONITOR_DATA';}})()"));
        capability_names.clear();
        auto created = callbacks.size();
        TRY(valid("(() => {try{badPowerHost.powerMonitorStart(1);return false;}"
                  "catch(e){return e.code==='ERR_FIREFLY_POWER_UNAVAILABLE';}})()"));
        check(callbacks.size() == created,
            "empty native capability snapshot cannot create observers");
        bad->stop();
        capability_names = original_capabilities;
        auto temporary_receiver = std::make_unique<QObject>();
        auto temporary = install("temporaryPowerHost"_utf16, *temporary_receiver,
            [&](std::string) { ++temporary_notifications; });
        TRY(modules.run_eval("temporaryPowerHost.powerMonitorStart(1)"));
        auto temporary_callback = callbacks.back();
        worker_event(temporary_callback, { "resume", {}, -1 });
        temporary->stop();
        temporary_receiver.reset();
        pump();
        worker_event(temporary_callback, { "resume", {}, -1 });
        pump();
        check(temporary_notifications == 0,
            "receiver teardown drops queued and subsequent worker events");
        return {};
    };

    hooks.shutdown = [&] {
        for (auto const& controller : controllers)
            controller->stop();
    };

    char executable[] = "firefly-power-monitor-host-probe";
    char option[] = "-e";
    char empty[] = "";
    char* arguments[] = { executable, option, empty, nullptr };
    auto status = Mite::run(3, arguments, hooks);
    controllers.clear();
    receiver.reset();
    for (auto const& callback : callbacks)
        worker_event(callback, { "resume", {}, -1 });
    pump();
    check(delivered.size() == 5 && temporary_notifications == 0,
        "callbacks after VM/controller teardown cannot notify");
    if (status || !passed)
        return 1;
    puts("PASS production power host native query validation, Unicode failures and cached "
         "capability integrity");
    puts("PASS production power host worker owner-thread frames, monotonic epochs, rollback and "
         "stale-stop isolation");
    puts("PASS production power host stop/receiver/VM teardown drops queued and late native "
         "callbacks");
    puts("Controlled native provider only: no OS sleep, lock, battery, thermal or power-state "
         "acceptance");
    return 0;
}
