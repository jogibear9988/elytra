/** Passive AppKit/Foundation/IOKit observers; never registers power acknowledgement hooks. */
#include "Provider.h"
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/pwr_mgt/IOPM.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#include <atomic>
#include <cmath>
#include <notify.h>
#include <utility>

namespace Firefly::PowerMonitor {
namespace {
    // Only observations of the actual session or lock/unlock notifications set
    // this value. Missing session metadata never implies an unlocked screen.
    std::atomic<int> observed_lock { -1 };
    std::atomic<unsigned> lock_observers { 0 };

    Result unavailable(char const* message)
    {
        return { 0, {}, false, "ERR_FIREFLY_POWER_UNAVAILABLE", message };
    }

    Result idle_time()
    {
        auto seconds = CGEventSourceSecondsSinceLastEventType(
            kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 9007199254740991.0)
            return unavailable("Native system idle time is unavailable");
        return { std::floor(seconds), {}, false, {}, {} };
    }

    int screen_locked()
    {
        auto session = CGSessionCopyCurrentDictionary();
        if (!session)
            return -1;
        auto console = CFDictionaryGetValue(session, kCGSessionOnConsoleKey);
        if (!console || CFGetTypeID(console) != CFBooleanGetTypeID()
            || !CFBooleanGetValue(static_cast<CFBooleanRef>(console))) {
            CFRelease(session);
            return -1;
        }

        int locked = -1;
        auto value = CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked"));
        if (value && CFGetTypeID(value) == CFBooleanGetTypeID())
            locked = CFBooleanGetValue(static_cast<CFBooleanRef>(value)) ? 1 : 0;
        CFRelease(session);
        return locked >= 0 ? locked : observed_lock.load();
    }

    Result battery_power()
    {
        auto info = IOPSCopyPowerSourcesInfo();
        if (!info)
            return unavailable("Native power source snapshot is unavailable");
        auto type = IOPSGetProvidingPowerSourceType(info);
        Result result = unavailable("Native power source is neither known battery nor AC");
        if (type && CFEqual(type, CFSTR(kIOPMBatteryPowerKey)))
            result = { 0, {}, true, {}, {} };
        else if (type && CFEqual(type, CFSTR(kIOPMACPowerKey)))
            result = { 0, {}, false, {}, {} };
        CFRelease(info);
        return result;
    }

    Result speed_limit()
    {
        CFDictionaryRef status = nullptr;
        auto error = IOPMCopyCPUPowerStatus(&status);
        if (error != kIOReturnSuccess || !status) {
            if (status)
                CFRelease(status);
            return unavailable("Native CPU speed limit is not published");
        }

        auto value = CFDictionaryGetValue(status, CFSTR(kIOPMCPUPowerLimitProcessorSpeedKey));
        double limit = -1;
        bool valid = value && CFGetTypeID(value) == CFNumberGetTypeID()
            && CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberDoubleType, &limit)
            && std::isfinite(limit) && limit >= 0 && limit <= 100 && std::floor(limit) == limit;
        CFRelease(status);
        if (!valid)
            return unavailable("Native CPU speed limit value is unavailable");
        return { limit, {}, false, {}, {} };
    }

    void on_main(std::function<void()> function)
    {
        if (NSThread.isMainThread)
            function();
        else
            dispatch_async(dispatch_get_main_queue(), ^{ function(); });
    }
}

Result query(Query kind, int threshold)
{
    @autoreleasepool {
        switch (kind) {
        case Query::IdleTime:
            return idle_time();
        case Query::IdleState: {
            if (threshold < 0)
                return { 0, {}, false, "ERR_INVALID_ARG_VALUE",
                    "Idle threshold must be nonnegative" };

            auto locked = screen_locked();
            if (locked == 1)
                return { 0, "locked", false, {}, {} };
            auto idle = idle_time();
            if (locked < 0 || !idle.code.empty())
                return { 0, "unknown", false, {}, {} };
            return { 0, idle.number >= threshold ? "idle" : "active", false, {}, {} };
        }

        case Query::ThermalState:
            switch (NSProcessInfo.processInfo.thermalState) {
            case NSProcessInfoThermalStateNominal:
                return { 0, "nominal", false, {}, {} };
            case NSProcessInfoThermalStateFair:
                return { 0, "fair", false, {}, {} };
            case NSProcessInfoThermalStateSerious:
                return { 0, "serious", false, {}, {} };
            case NSProcessInfoThermalStateCritical:
                return { 0, "critical", false, {}, {} };
            }

            return { 0, "unknown", false, {}, {} };
        case Query::BatteryPower:
            return battery_power();
        case Query::SpeedLimit:
            return speed_limit();
        }

        return unavailable("Unknown native power query");
    }
}

namespace {
    struct State : std::enable_shared_from_this<State> {
        struct Observer {
            NSNotificationCenter* __strong center;
            id __strong token;
        };

        explicit State(EventCallback value)
            : callback(std::move(value))
        {
        }

        EventCallback callback;
        std::vector<Observer> observers;
        CFRunLoopSourceRef power_source {};
        int cpu_token { -1 };
        bool stopped {}, started {}, battery_known {}, last_battery {}, lock_registered {};

        void emit(Event event)
        {
            if (stopped || !started || !callback)
                return;
            auto notify = callback;
            notify(std::move(event));
        }

        void battery_changed()
        {
            if (stopped || !started)
                return;
            auto power = query(Query::BatteryPower);
            if (!power.code.empty())
                return;
            bool changed = battery_known && power.flag != last_battery;
            last_battery = power.flag;
            battery_known = true;
            if (changed)
                emit({ power.flag ? "on-battery" : "on-ac", {}, -1 });
        }

        void observe(NSNotificationCenter* center, NSNotificationName name, char const* type)
        {
            auto weak = weak_from_this();
            std::string event(type);
            auto token = [center addObserverForName:name
                                             object:nil
                                              queue:nil
                                         usingBlock:^(NSNotification*) {
                                             on_main([weak, event] {
                                                 auto state = weak.lock();
                                                 if (!state || state->stopped || !state->started)
                                                     return;
                                                 if (event == "lock-screen")
                                                     observed_lock.store(1);
                                                 else if (event == "unlock-screen")
                                                     observed_lock.store(0);
                                                 if (event == "thermal-state-change") {
                                                     auto result = query(Query::ThermalState);
                                                     if (result.code.empty())
                                                         state->emit({ event, result.text, -1 });
                                                 } else
                                                     state->emit({ event, {}, -1 });
                                             });
                                         }];
            observers.push_back({ center, token });
        }

        void cleanup()
        {
            for (auto const& observer : observers)
                [observer.center removeObserver:observer.token];
            observers.clear();
            if (power_source) {
                CFRunLoopRemoveSource(CFRunLoopGetMain(), power_source, kCFRunLoopCommonModes);
                CFRunLoopSourceInvalidate(power_source);
                CFRelease(power_source);
                power_source = nullptr;
            }

            if (cpu_token >= 0) {
                notify_cancel(cpu_token);
                cpu_token = -1;
            }

            started = false;
            if (lock_registered) {
                lock_registered = false;
                if (lock_observers.fetch_sub(1) == 1)
                    observed_lock.store(-1);
            }
        }

        Result start()
        {
            if (!NSThread.isMainThread)
                return unavailable("Native power observers require the main owner thread");
            if (stopped)
                return unavailable("Native power observer has stopped");
            if (started)
                return {};
            auto workspace = NSWorkspace.sharedWorkspace.notificationCenter;
            observe(workspace, NSWorkspaceWillSleepNotification, "suspend");
            observe(workspace, NSWorkspaceDidWakeNotification, "resume");
            observe(workspace, NSWorkspaceWillPowerOffNotification, "shutdown");
            observe(
                workspace, NSWorkspaceSessionDidBecomeActiveNotification, "user-did-become-active");
            observe(
                workspace, NSWorkspaceSessionDidResignActiveNotification, "user-did-resign-active");
            auto distributed = NSDistributedNotificationCenter.defaultCenter;
            observe(distributed, @"com.apple.screenIsLocked", "lock-screen");
            observe(distributed, @"com.apple.screenIsUnlocked", "unlock-screen");
            observe(NSNotificationCenter.defaultCenter,
                NSProcessInfoThermalStateDidChangeNotification, "thermal-state-change");
            for (auto const& observer : observers)
                if (!observer.token) {
                    cleanup();
                    return unavailable("Native power notification registration failed");
                }

            auto power = query(Query::BatteryPower);
            battery_known = power.code.empty();
            if (battery_known) {
                last_battery = power.flag;
                power_source = IOPSNotificationCreateRunLoopSource(
                    [](void* context) {
                        auto state = static_cast<State*>(context)->shared_from_this();
                        state->battery_changed();
                    },
                    this);
                if (!power_source) {
                    cleanup();
                    return unavailable("Native power source observation is unavailable");
                }

                CFRunLoopAddSource(CFRunLoopGetMain(), power_source, kCFRunLoopCommonModes);
            }

            auto cpu = query(Query::SpeedLimit);
            if (cpu.code.empty()) {
                auto weak = weak_from_this();
                auto status = notify_register_dispatch(
                    kIOPMCPUPowerNotificationKey, &cpu_token, dispatch_get_main_queue(), ^(int) {
                        auto state = weak.lock();
                        if (!state || state->stopped || !state->started)
                            return;
                        auto limit = query(Query::SpeedLimit);
                        if (limit.code.empty())
                            state->emit(
                                { "speed-limit-change", {}, static_cast<int>(limit.number) });
                    });
                if (status != NOTIFY_STATUS_OK) {
                    cpu_token = -1;
                    cleanup();
                    return unavailable("Native CPU power observation is unavailable");
                }
            }

            started = true;
            lock_observers.fetch_add(1);
            lock_registered = true;
            return {};
        }

        void stop()
        {
            if (stopped)
                return;
            stopped = true;
            callback = {};
            cleanup();
        }
    };

    class MacProvider final : public Provider {
    public:
        explicit MacProvider(EventCallback callback)
            : state(std::make_shared<State>(std::move(callback)))
        {
        }

        ~MacProvider() override
        {
            stop();
        }

        Result start() override
        {
            return state->start();
        }

        void stop() override
        {
            on_main([value = state] { value->stop(); });
        }

    private:
        std::shared_ptr<State> state;
    };
}

std::vector<std::string> Provider::capabilities()
{
    std::vector<std::string> result { "suspend", "resume", "shutdown", "lock-screen",
        "unlock-screen", "user-did-become-active", "user-did-resign-active",
        "thermal-state-change" };

    if (query(Query::BatteryPower).code.empty()) {
        result.push_back("on-ac");
        result.push_back("on-battery");
    }

    if (query(Query::SpeedLimit).code.empty())
        result.push_back("speed-limit-change");
    return result;
}

std::shared_ptr<Provider> Provider::create(EventCallback callback)
{
    return std::make_shared<MacProvider>(std::move(callback));
}
}
