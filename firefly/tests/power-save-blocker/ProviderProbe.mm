/** Exact production provider with executable-owned IOPM symbols. No OS assertion is created. */
#include "native/power-save-blocker/Provider.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
using Firefly::PowerSaveBlocker::Provider;
using Firefly::PowerSaveBlocker::Type;
bool passed = true;
bool fail_create = false;
std::unordered_set<IOPMAssertionID> fail_release;
std::unordered_map<IOPMAssertionID, Type> active;
std::vector<std::string> operations;
IOPMAssertionID sequence = 0;

void check(bool condition, char const* label)
{
    if (!condition) {
        fprintf(stderr, "FAIL power-save provider: %s\n", label);
        passed = false;
    }
}

void reset()
{
    check(active.empty(), "previous provider retired all controlled assertions");
    fail_create = false;
    fail_release.clear();
    operations.clear();
    sequence = 0;
}
}

// These exact executable definitions satisfy the production provider's undefined
// IOPM references. No original function pointer or dynamic OS call is used.
extern "C" IOReturn IOPMAssertionCreateWithName(
    CFStringRef type, IOPMAssertionLevel level, CFStringRef name, IOPMAssertionID* identifier)
{
    check(level == kIOPMAssertionLevelOn && name && CFStringGetLength(name) > 0,
        "native create carries actual enabled level and bounded app description");
    bool display = CFEqual(type, kIOPMAssertionTypeNoDisplaySleep);
    check(display || CFEqual(type, kIOPMAssertionTypeNoIdleSleep),
        "only allowed user-idle assertion types");
    operations.push_back(display ? "create-display" : "create-app");
    if (fail_create)
        return kIOReturnError;
    *identifier = ++sequence;
    active.emplace(*identifier, display ? Type::DisplaySleep : Type::AppSuspension);
    return kIOReturnSuccess;
}

extern "C" IOReturn IOPMAssertionRelease(IOPMAssertionID identifier)
{
    operations.push_back("release-" + std::to_string(identifier));
    check(active.contains(identifier), "release only currently owned native assertion IDs");
    if (fail_release.contains(identifier))
        return kIOReturnError;
    active.erase(identifier);
    return kIOReturnSuccess;
}

int main()
{
    {
        auto provider = Provider::create();
        check(operations.empty(), "construction cannot assert OS power policy");
        auto first = provider->start(Type::AppSuspension);
        auto second = provider->start(Type::AppSuspension);
        check(first.code.empty() && first.id == 0 && second.id == 1 && active.size() == 1,
            "logical IDs start at zero, same-type requests share one assertion");
        check(provider->is_started(0).flag && provider->is_started(1).flag,
            "each logical blocker is independently owned");
        auto display = provider->start(Type::DisplaySleep);
        check(display.id == 2 && active.size() == 1 && active.begin()->second == Type::DisplaySleep,
            "display request takes precedence");
        check(operations.size() == 3 && operations[1] == "create-display"
                && operations[2] == "release-1",
            "promotion acquires replacement before releasing original");
        check(provider->stop(first.id).flag && active.size() == 1,
            "stopping app request does not downgrade active display request");
        check(provider->stop(display.id).flag && active.size() == 1
                && active.begin()->second == Type::AppSuspension,
            "last display stop restores surviving app requirement");
        check(!provider->stop(display.id).flag && !provider->is_started(display.id).flag,
            "unknown/repeated stop is false");
        check(provider->stop(second.id).flag && active.empty(),
            "final stop releases the aggregate assertion");
        check(!provider->is_started(-1).flag && !provider->stop(1000).flag,
            "unowned IDs remain false");
        auto restarted = provider->start(Type::AppSuspension);
        check(restarted.id == 3, "successful logical IDs never reuse retired IDs");
        provider->shutdown();
        check(active.empty() && !provider->is_started(restarted.id).flag,
            "shutdown retires native and logical ownership");
        check(provider->start(Type::DisplaySleep).code == "ERR_FIREFLY_APP_EXITED",
            "shutdown permanently refuses new requests");
    }

    puts("PASS native power-save provider logical identity, aggregate precedence, replacement "
         "order and shutdown");
    reset();
    {
        auto provider = Provider::create();
        check(provider->start(static_cast<Type>(99)).code == "ERR_INVALID_ARG_VALUE"
                && active.empty(),
            "unknown native type cannot create an assertion");
        for (size_t index = 0; index < Provider::request_limit; ++index)
            check(provider->start(Type::AppSuspension).id == static_cast<int>(index),
                "bounded logical ID sequence");
        auto calls = operations.size();
        check(provider->start(Type::DisplaySleep).code == "ERR_FIREFLY_POWER_SAVE_LIMIT"
                && operations.size() == calls && active.size() == 1,
            "logical quota rejects before additional native assertions");
        provider->shutdown();
    }

    reset();
    {
        auto provider = Provider::create();
        auto original = provider->start(Type::AppSuspension);
        fail_create = true;
        check(provider->start(Type::DisplaySleep).code == "ERR_FIREFLY_POWER_SAVE_CREATE"
                && provider->is_started(original.id).flag && active.size() == 1,
            "failed replacement creation preserves original logical/native protection");
        fail_create = false;
        fail_release = { 1, 2 };
        check(provider->start(Type::DisplaySleep).code == "ERR_FIREFLY_POWER_SAVE_RELEASE"
                && active.size() == 2 && provider->is_started(original.id).flag
                && !provider->is_started(1).flag,
            "failed release and rollback retain both known IDs without committing logical request");
        auto calls = operations.size();
        for (size_t index = 0; index < 100; ++index)
            check(provider->start(Type::DisplaySleep).code == "ERR_FIREFLY_POWER_SAVE_RELEASE",
                "degraded rollback ownership refuses new mutation");
        check(active.size() == 2 && sequence == 2 && operations.size() == calls + 100,
            "repeated failed promotions retry cleanup without accumulating assertions");
        fail_release.clear();
        auto promoted = provider->start(Type::DisplaySleep);
        check(promoted.code.empty() && promoted.id == 1 && active.size() == 1
                && active.begin()->second == Type::DisplaySleep,
            "successful cleanup permits the next atomic promotion");
        check(
            provider->stop(promoted.id).flag && provider->stop(original.id).flag && active.empty(),
            "successful downgrade and final release retire all recovered ownership");
    }

    reset();
    {
        auto provider = Provider::create();
        auto logical = provider->start(Type::AppSuspension);
        fail_release.insert(1);
        check(provider->stop(logical.id).code == "ERR_FIREFLY_POWER_SAVE_RELEASE"
                && provider->is_started(logical.id).flag && active.size() == 1,
            "failed final release does not report stopped or erase its logical request");
        provider->shutdown();
        check(!provider->is_started(logical.id).flag && active.size() == 1,
            "shutdown revokes logical API while retaining failed native release ownership");
        fail_release.clear();
        provider.reset();
        check(active.empty(), "destructor retries retained shutdown release");
    }

    puts("PASS native power-save provider native failure atomicity, bounded rollback cleanup and "
         "destructor retry");
    reset();
    {
        auto provider = Provider::create();
        provider->start(Type::AppSuspension);
        fail_release.insert(1);
        auto before = operations.size();
        provider.reset();
        check(operations.size() <= before + 4 && active.size() == 1,
            "permanent native failure has bounded best-effort destructor retries");
        // This is a controlled peer's bookkeeping only, not an OS assertion.
        // Permanent provider failure is intentionally not certified as cleanup success.
        fail_release.clear();
        active.clear();
    }

    if (!passed)
        return 1;
    puts(
        "Controlled IOPM symbols only: no OS assertion, sleep change or physical power acceptance");
    return 0;
}
