/** Actual production CF reader/serial queue against controlled native peers, or passive OS reads.
 */
#include "platform/policy/ProviderMacTesting.h"
#include <AK/Array.h>
#include <CoreFoundation/CoreFoundation.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace P = Mite::Policy;

namespace {
using namespace std::chrono_literals;

ErrorOr<void> check(bool value, char const* message)
{
    if (!value)
        return Error::from_string_view(StringView(message, std::strlen(message)));
    return {};
}

struct Peer final : P::PreferencesPeer {
    std::mutex mutex;
    std::map<std::string, CFPropertyListRef> values;
    std::set<std::string> forced;
    std::function<void()> changed, on_read;
    std::atomic<unsigned> starts {}, stops {}, reads {}, completed_reads {};
    bool fail_start {}, stopped {};
    std::thread::id owner = std::this_thread::get_id();
    std::atomic<bool> wrong_thread {};

    ~Peer() override
    {
        for (auto const& [key, value] : values) {
            (void)key;
            CFRelease(value);
        }
    }

    static std::string name(CFStringRef key)
    {
        char text[128] {};
        if (!CFStringGetCString(key, text, sizeof(text), kCFStringEncodingUTF8))
            return {};
        return text;
    }

    void put(std::string key, CFPropertyListRef value)
    {
        std::lock_guard lock(mutex);
        auto found = values.find(key);
        if (found != values.end()) {
            CFRelease(found->second);
            values.erase(found);
        }

        if (value)
            values.emplace(std::move(key), CFRetain(value));
    }

    void notify()
    {
        std::function<void()> callback;
        {
            std::lock_guard lock(mutex);
            callback = changed;
        }

        if (callback)
            callback();
    }

    CFPropertyListRef copy_value(CFStringRef key, CFStringRef) override
    {
        if (std::this_thread::get_id() == owner)
            wrong_thread = true;
        ++reads;
        CFPropertyListRef value = nullptr;
        std::function<void()> action;
        {
            std::lock_guard lock(mutex);
            auto found = values.find(name(key));
            if (found != values.end())
                value = CFRetain(found->second);
            action = on_read;
        }

        if (action)
            action();
        ++completed_reads;
        return value;
    }

    bool is_forced(CFStringRef key, CFStringRef) override
    {
        std::lock_guard lock(mutex);
        return forced.contains(name(key));
    }

    ErrorOr<void> start(dispatch_queue_t, std::function<void()> callback) override
    {
        ++starts;
        std::lock_guard lock(mutex);
        changed = std::move(callback);
        if (fail_start)
            return Error::from_errno(EACCES);
        return {};
    }

    void stop() override
    {
        std::lock_guard lock(mutex);
        if (stopped)
            return;
        stopped = true;
        ++stops;
        changed = {};
    }
};

P::Definition definition(char const* name, P::Type type, bool union_type = false)
{
    return { ByteString(name), { type }, union_type };
}

ErrorOr<P::Snapshot> next(P::Provider& provider)
{
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        auto snapshot = TRY(provider.take_snapshot());
        if (snapshot.has_value())
            return snapshot.release_value();
        std::this_thread::sleep_for(1ms);
    }

    return Error::from_string_literal("Native policy snapshot timed out");
}

ErrorOr<void> controlled()
{
    auto peer = std::make_shared<Peer>();
    peer->put("String", CFSTR("Grüße 日本語"));
    peer->put("Boolean", kCFBooleanTrue);
    long long integer = 9007199254740993LL;
    auto number = CFNumberCreate(nullptr, kCFNumberLongLongType, &integer);
    peer->put("Number", number);
    CFRelease(number);
    peer->put("WrongBoolean", CFSTR("true"));
    peer->put("WrongNumber", kCFBooleanTrue);
    peer->put("UnionHidden", CFSTR("unmanaged"));
    peer->put("UnionForced", CFSTR("managed"));
    peer->forced.insert("UnionForced");
    Vector<P::Definition> definitions { definition("String", P::Type::String),
        definition("Boolean", P::Type::Boolean), definition("Number", P::Type::Number),
        definition("Missing", P::Type::String), definition("WrongBoolean", P::Type::Boolean),
        definition("WrongNumber", P::Type::Number),
        definition("UnionHidden", P::Type::String, true),
        definition("UnionForced", P::Type::String, true) };
    auto provider = TRY(P::create_provider_for_testing(
        "org.elytra.policy.controlled", std::move(definitions), peer));
    auto first = TRY(next(*provider));
    TRY(check(first.size() == 4 && first.get("String")->get<ByteString>() == "Grüße 日本語"sv
            && first.get("Boolean")->get<bool>()
            && first.get("Number")->get<double>() == static_cast<double>(integer)
            && first.get("UnionForced")->get<ByteString>() == "managed"sv
            && !first.contains("UnionHidden") && !first.contains("WrongBoolean")
            && !first.contains("WrongNumber") && !peer->wrong_thread,
        "Exact scalar/union CF semantics and native worker ownership"));
    TRY(check(!TRY(provider->take_snapshot()).has_value(), "Consumed snapshot published twice"));
    peer->put("String", nullptr);
    peer->put("Boolean", kCFBooleanFalse);
    peer->notify();
    auto second = TRY(next(*provider));
    TRY(check(
        second.size() == 3 && !second.contains("String") && !second.get("Boolean")->get<bool>(),
        "Actual full snapshots preserve removal and unchanged values"));
    // Exercise latest-state coalescing, not a queue of obsolete snapshots.
    peer->put("String", CFSTR("latest"));
    for (int i = 0; i < 1000; ++i)
        peer->notify();
    auto coalesced = TRY(next(*provider));
    TRY(check(
        coalesced.get("String")->get<ByteString>() == "latest"sv, "Latest coalesced snapshot"));
    std::function<void()> late;
    {
        std::lock_guard lock(peer->mutex);
        late = peer->changed;
    }

    provider->stop();
    provider->stop();
    auto before = peer->reads.load();
    if (late)
        late();
    std::this_thread::sleep_for(10ms);
    TRY(check(
        peer->stops == 1 && peer->reads == before && !TRY(provider->take_snapshot()).has_value(),
        "Stop drains native queue, clears data and revokes late callbacks"));
    std::puts("PASS controlled CF policy scalars/unions, worker snapshots, removals/coalescing and "
              "idempotent drained shutdown");
    return {};
}

ErrorOr<void> failures()
{
    auto peer = std::make_shared<Peer>();
    peer->fail_start = true;
    auto failed = P::create_provider_for_testing("org.elytra.policy.controlled", {}, peer);
    TRY(check(failed.is_error() && peer->starts == 1 && peer->stops == 1,
        "Native watcher start failure must not masquerade as empty policy"));
    auto limited = std::make_shared<Peer>();
    std::string large(P::value_limit + 1, 'x');
    auto text = CFStringCreateWithBytes(nullptr, reinterpret_cast<UInt8 const*>(large.data()),
        large.size(), kCFStringEncodingUTF8, false);
    limited->put("String", text);
    CFRelease(text);
    auto provider = TRY(P::create_provider_for_testing(
        "org.elytra.policy.limit", { definition("String", P::Type::String) }, limited));
    // Wait until the real worker observed the excessive value, then publish a valid
    // update before polling. The first error must remain latched across coalescing.
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!limited->completed_reads && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(10ms);
    limited->put("String", CFSTR("later valid"));
    limited->notify();
    std::this_thread::sleep_for(10ms);
    bool observed_error = false;
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = provider->take_snapshot();
        if (result.is_error()) {
            observed_error = true;
            break;
        }

        TRY(check(!result.value().has_value(), "Native failure overwritten by valid snapshot"));
        std::this_thread::sleep_for(1ms);
    }

    TRY(check(observed_error, "Native failure lost behind later successful snapshot"));
    limited->notify();
    std::this_thread::sleep_for(10ms);
    TRY(check(
        !TRY(provider->take_snapshot()).has_value(), "Failed provider resumed data publication"));
    provider->stop();
    std::puts("PASS native policy start errors, value quota and latched failure across later "
              "notifications");
    return {};
}

ErrorOr<void> reentrant_stop()
{
    auto peer = std::make_shared<Peer>();
    auto provider = TRY(P::create_provider_for_testing(
        "org.elytra.policy.stop", { definition("String", P::Type::String) }, peer));
    (void)TRY(next(*provider));
    {
        std::lock_guard lock(peer->mutex);
        peer->on_read = [&] { provider->stop(); };
    }

    peer->notify();
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!peer->stops && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    TRY(check(peer->stops == 1 && !TRY(provider->take_snapshot()).has_value(),
        "Stopping on provider serial queue must not deadlock or publish stale data"));
    return {};
}

ErrorOr<void> real_read_only()
{
    auto provider = TRY(P::Provider::create("org.elytra.mite.native-policy-read-only-probe",
        { definition("UnconfiguredProbe", P::Type::Boolean) }));
    auto observed = TRY(next(*provider));
    for (auto const& entry : observed)
        TRY(check(
            entry.key == "UnconfiguredProbe"sv && entry.value.has<bool>(), "Actual CF query type"));
    provider->stop();
    provider->stop();
    std::puts("PASS real macOS managed-preference observation and initial asynchronous CF "
              "snapshot; no preference writes, GUI or permission requests");
    return {};
}
}

int main(int argc, char** argv)
{
    auto result
        = argc == 2 && std::string(argv[1]) == "--read-only" ? real_read_only() : controlled();
    if (!result.is_error() && argc == 1)
        result = failures();
    if (!result.is_error() && argc == 1)
        result = reentrant_stop();
    if (result.is_error()) {
        warnln("FAIL native policy provider: {}", result.error());
        return 1;
    }

    return 0;
}
