/** Compare genuine read-only OS queries and passive subscription cleanup; no power/input changes.
 */
#include "native/power-monitor/Provider.h"
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/pwr_mgt/IOPM.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Firefly::PowerMonitor;

int main()
{
    @autoreleasepool {
        auto before = std::floor(CGEventSourceSecondsSinceLastEventType(
            kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType));
        auto idle = query(Query::IdleTime);
        auto after = std::floor(CGEventSourceSecondsSinceLastEventType(
            kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType));
        if (!idle.code.empty() || idle.number < std::min(before, after)
            || idle.number > std::max(before, after) + 1)
            return 1;
        auto state = query(Query::IdleState);
        if (!state.code.empty()
            || (state.text != "unknown" && state.text != "locked" && state.text != "active"
                && state.text != "idle"))
            return 2;
        char const* thermal = "unknown";
        switch (NSProcessInfo.processInfo.thermalState) {
        case NSProcessInfoThermalStateNominal:
            thermal = "nominal";
            break;
        case NSProcessInfoThermalStateFair:
            thermal = "fair";
            break;
        case NSProcessInfoThermalStateSerious:
            thermal = "serious";
            break;
        case NSProcessInfoThermalStateCritical:
            thermal = "critical";
            break;
        }

        if (query(Query::ThermalState).text != thermal)
            return 3;
        auto power = query(Query::BatteryPower);
        auto info = IOPSCopyPowerSourcesInfo();
        auto type = info ? IOPSGetProvidingPowerSourceType(info) : nullptr;
        if (type && CFEqual(type, CFSTR(kIOPMBatteryPowerKey))) {
            if (!power.code.empty() || !power.flag)
                return 4;
        } else if (type && CFEqual(type, CFSTR(kIOPMACPowerKey))) {
            if (!power.code.empty() || power.flag)
                return 5;
        } else if (power.code.empty())
            return 6;
        if (info)
            CFRelease(info);
        auto speed = query(Query::SpeedLimit);
        CFDictionaryRef cpu = nullptr;
        auto status = IOPMCopyCPUPowerStatus(&cpu);
        auto value = status == kIOReturnSuccess && cpu
            ? CFDictionaryGetValue(cpu, CFSTR(kIOPMCPUPowerLimitProcessorSpeedKey))
            : nullptr;
        double native_limit = -1;
        bool published = value && CFGetTypeID(value) == CFNumberGetTypeID()
            && CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberDoubleType, &native_limit)
            && native_limit >= 0 && native_limit <= 100 && std::floor(native_limit) == native_limit;
        if (cpu)
            CFRelease(cpu);
        if (published ? !speed.code.empty() || speed.number != native_limit : speed.code.empty())
            return 7;
        auto capabilities = Provider::capabilities();
        if (std::find(capabilities.begin(), capabilities.end(), "speed-limit-change")
            == capabilities.end()) {
            fprintf(stderr, "FAIL CPU observation cannot depend on the current published value\n");
            return 11;
        }
        // Register/cancel the genuine passive CPU source even when the read-only
        // query above reports that the system has not published a value yet.
        // This does not synthesize notifications or induce a power transition.
        auto provider = Provider::create([](Event) { });
        auto started = provider->start();
        if (!started.code.empty()) {
            fprintf(stderr, "FAIL passive power subscriptions: %s\n", started.message.c_str());
            return 8;
        }

        if (!provider->start().code.empty())
            return 9;
        provider->stop();
        provider->stop();
        if (provider->start().code.empty())
            return 10;
        puts("PASS native idle/thermal/power/CPU observations and passive observer start/stop; no "
             "sleep/lock/input/preferences changed");
    }

    return 0;
}
