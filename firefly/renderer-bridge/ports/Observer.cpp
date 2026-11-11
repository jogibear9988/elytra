/** Process-local, weak observation ledger. Rust finalizers only update native atomics. */
#include "Observer.h"
#include <AK/Random.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/HTML/MessagePort.h>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>

extern "C" {
bool firefly_message_port_observe(
    void* vm, void* port, uint64_t origin_high, uint64_t origin_low, uint64_t token, bool consume);
WEB_API bool firefly_port_observer_bind(void* vm, void* realm, void* port, uint64_t origin_high,
    uint64_t origin_low, uint64_t token, bool consume);
WEB_API void firefly_port_observer_notify(
    uint64_t origin_high, uint64_t origin_low, uint64_t token, uint8_t kind);
}

namespace Firefly::PortObservation {
namespace {
    struct Entry {
        std::weak_ptr<State> state;
        JS::VM* vm;
        std::thread::id thread;
        void* context;
        Validator validator;
    };

    struct Registry {
        Registry()
        {
            do {
                origin_high = get_random<uint64_t>();
                origin_low = get_random<uint64_t>();
            } while (!origin_high && !origin_low);
        }

        std::mutex mutex;
        uint64_t origin_high {};
        uint64_t origin_low {};
        uint64_t sequence { 0 };
        std::unordered_map<uint64_t, Entry> entries;
    };

    Registry& registry()
    {
        // Native process-lifetime metadata: late Rust finalizers must not race
        // static destruction. Entries are weak and capped, never GC objects.
        static auto* value = new Registry;
        return *value;
    }

    struct Lookup {
        std::shared_ptr<State> state;
        Entry entry;
    };

    Lookup lookup(uint64_t high, uint64_t low, uint64_t token)
    {
        auto& value = registry();
        std::lock_guard lock(value.mutex);
        if (high != value.origin_high || low != value.origin_low || !token)
            return {};
        auto found = value.entries.find(token);
        if (found == value.entries.end())
            return {};
        auto state = found->second.state.lock();
        return { std::move(state), found->second };
    }
}

std::shared_ptr<State> create(JS::VM& vm, void* context, Validator validator)
{
    if (!context || !validator)
        return {};
    auto& value = registry();
    std::lock_guard lock(value.mutex);
    for (auto it = value.entries.begin(); it != value.entries.end();) {
        auto state = it->second.state.lock();
        if (!state || state->closed.load())
            it = value.entries.erase(it);
        else
            ++it;
    }

    if (value.entries.size() >= 16384 || value.sequence == std::numeric_limits<uint64_t>::max())
        return {};
    auto state = std::make_shared<State>(++value.sequence);
    value.entries.emplace(
        state->token, Entry { state, &vm, std::this_thread::get_id(), context, validator });
    return state;
}

bool observe(Web::HTML::MessagePort& port, std::shared_ptr<State> const& state, bool consume)
{
    if (!state || state->closed.load())
        return false;
    auto& value = registry();
    auto found = lookup(value.origin_high, value.origin_low, state->token);
    if (found.state != state || found.entry.thread != std::this_thread::get_id())
        return false;
    return firefly_message_port_observe(
        found.entry.vm, &port, value.origin_high, value.origin_low, state->token, consume);
}

void revoke(std::shared_ptr<State> const& state)
{
    if (!state)
        return;
    state->closed.store(true);
    auto& value = registry();
    std::lock_guard lock(value.mutex);
    auto found = value.entries.find(state->token);
    if (found != value.entries.end() && found->second.state.lock() == state)
        value.entries.erase(found);
}
}

bool firefly_port_observer_bind(
    void* vm, void* realm, void* port, uint64_t high, uint64_t low, uint64_t token, bool)
{
    using namespace Firefly::PortObservation;
    auto found = lookup(high, low, token);
    if (!found.state || found.state->closed.load())
        return false;
    // Reject a foreign agent before reading any realm, VM, port or context.
    if (found.entry.thread != std::this_thread::get_id() || found.entry.vm != vm || !realm
        || !port) {
        found.state->closed.store(true);
        return false;
    }

    auto& actual_realm = *static_cast<JS::Realm*>(realm);
    if (&actual_realm.vm() != found.entry.vm
        || !found.entry.validator(
            found.entry.context, *static_cast<Web::HTML::MessagePort*>(port), actual_realm)) {
        found.state->closed.store(true);
        return false;
    }

    return !found.state->closed.load();
}

void firefly_port_observer_notify(uint64_t high, uint64_t low, uint64_t token, uint8_t kind)
{
    using namespace Firefly::PortObservation;
    auto found = lookup(high, low, token);
    if (!found.state || found.state->closed.load())
        return;
    if (kind == 2) {
        found.state->closed.store(true);
    } else if (kind == 1) {
        auto count = found.state->dequeued.load();
        while (count != std::numeric_limits<uint64_t>::max()) {
            if (found.state->dequeued.compare_exchange_weak(count, count + 1))
                return;
        }

        found.state->closed.store(true);
    }
}
