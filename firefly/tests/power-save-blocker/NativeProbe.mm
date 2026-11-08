/** Opt-in actual IOPM assertion lifecycle. Never triggers sleep, locks or changes preferences. */
#include "native/power-save-blocker/Provider.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <cstdio>
#include <unistd.h>
#include <vector>

namespace {
using Firefly::PowerSaveBlocker::Provider;
using Firefly::PowerSaveBlocker::Type;

class Snapshot {
public:
    ~Snapshot()
    {
        if (m_value)
            CFRelease(m_value);
    }

    bool read(std::vector<Type>& types)
    {
        if (IOPMCopyAssertionsByProcess(&m_value) != kIOReturnSuccess || !m_value)
            return false;
        auto pid = getpid();
        auto number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
        auto raw = CFDictionaryGetValue(m_value, number);
        CFRelease(number);
        if (!raw)
            return true;
        if (CFGetTypeID(raw) != CFArrayGetTypeID())
            return false;
        auto assertions = static_cast<CFArrayRef>(raw);
        for (CFIndex index = 0; index < CFArrayGetCount(assertions); ++index) {
            auto value = CFArrayGetValueAtIndex(assertions, index);
            if (CFGetTypeID(value) != CFDictionaryGetTypeID())
                return false;
            auto dictionary = static_cast<CFDictionaryRef>(value);
            auto name = CFDictionaryGetValue(dictionary, kIOPMAssertionNameKey);
            if (!name || CFGetTypeID(name) != CFStringGetTypeID()
                || !CFEqual(name, CFSTR("Firefly powerSaveBlocker")))
                continue;
            auto type = CFDictionaryGetValue(dictionary, kIOPMAssertionTypeKey);
            if (!type || CFGetTypeID(type) != CFStringGetTypeID())
                return false;
            if (CFEqual(type, kIOPMAssertionTypeNoIdleSleep)
                || CFEqual(type, kIOPMAssertionTypePreventUserIdleSystemSleep))
                types.push_back(Type::AppSuspension);
            else if (CFEqual(type, kIOPMAssertionTypeNoDisplaySleep)
                || CFEqual(type, kIOPMAssertionTypePreventUserIdleDisplaySleep))
                types.push_back(Type::DisplaySleep);
            else
                return false;
        }

        return true;
    }

private:
    CFDictionaryRef m_value {};
};

bool expect(size_t count, Type type = Type::AppSuspension)
{
    Snapshot snapshot;
    std::vector<Type> types;
    if (!snapshot.read(types) || types.size() != count || (count && types[0] != type)) {
        fputs("Actual owned IOPM assertion snapshot did not match requested state\n", stderr);
        return false;
    }

    return true;
}

struct Shutdown {
    Provider& provider;

    ~Shutdown()
    {
        provider.shutdown();
    }
};
}

int main()
{
    // Read only this process's matching description. Never release an assertion
    // learned from a global snapshot or owned by another application/library.
    if (!expect(0))
        return 1;
    auto provider = Provider::create();
    Shutdown cleanup { *provider };
    auto first = provider->start(Type::AppSuspension);
    if (!first.code.empty() || !provider->is_started(first.id).flag || !expect(1))
        return 1;
    auto second = provider->start(Type::AppSuspension);
    if (!second.code.empty() || !expect(1))
        return 1;
    auto display = provider->start(Type::DisplaySleep);
    if (!display.code.empty() || !expect(1, Type::DisplaySleep))
        return 1;
    auto stopped = provider->stop(display.id);
    if (!stopped.code.empty() || !stopped.flag || !expect(1))
        return 1;
    stopped = provider->stop(first.id);
    if (!stopped.code.empty() || !stopped.flag || !expect(1))
        return 1;
    stopped = provider->stop(second.id);
    if (!stopped.code.empty() || !stopped.flag || !expect(0))
        return 1;
    auto final = provider->start(Type::DisplaySleep);
    if (!final.code.empty() || !expect(1, Type::DisplaySleep))
        return 1;
    provider->shutdown();
    if (provider->is_started(final.id).flag || !expect(0))
        return 1;
    puts("PASS actual macOS owned IOPM create/aggregate/promote/downgrade/stop/shutdown assertion "
         "snapshots");
    puts("Transient owned idle-sleep assertions only: no forced sleep, lock, preference or "
         "physical sleep test");
    return 0;
}
