/** Exact production policy Host with controlled plain providers; never OS policy or preference
 * writes. */
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "platform/policy/Host.h"
#include <AK/JsonObject.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PromiseCapability.h>
#include <LibJS/Runtime/PromiseConstructor.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
struct IdleCollectionState : Mite::NativeCaptureOwner<IdleCollectionState> {
    GC::Root<JS::PromiseCapability> capability;

    void visit_capture_edges(GC::Cell::Visitor& visitor) const
    {
        visitor.visit(capability.ptr());
    }
};

using namespace Mite::Policy;

struct Feed {
    std::deque<int> steps;
    bool stopped {};
};

std::vector<std::shared_ptr<Feed>> feeds;
std::vector<GC::Weak<JS::FunctionObject>> tracked;
std::thread::id owner_thread;
int created {}, stopped {}, destroyed {}, polled {}, mode {};
bool passed = true;

class ControlledProvider final : public Provider {
public:
    explicit ControlledProvider(std::shared_ptr<Feed> feed)
        : m_feed(std::move(feed))
    {
        ++created;
    }

    ~ControlledProvider() override
    {
        stop();
        ++destroyed;
    }

    ErrorOr<Optional<Snapshot>> take_snapshot() override
    {
        if (std::this_thread::get_id() != owner_thread || m_feed->stopped)
            passed = false;
        ++polled;
        if (m_feed->steps.empty())
            return Optional<Snapshot> {};
        int step = m_feed->steps.front();
        m_feed->steps.pop_front();
        if (step == 3)
            return Error::from_string_literal("Controlled policy read failure");
        Snapshot snapshot;
        if (step == 1) {
            snapshot.set("Text", ByteString("managed Grüße 日本語"));
            snapshot.set("Count", 7.5);
            snapshot.set("Enabled", true);
            snapshot.set("Mixed", ByteString("union"));
        } else if (step == 2)
            snapshot.set("Count", 8.0);
        else if (step == 4)
            snapshot.set("Unexpected", true);
        else if (step == 5)
            snapshot.set("Text", false);
        else if (step == 6)
            snapshot.set("Text", ByteString("\xff", 1));
        else if (step == 7)
            snapshot.set("Count", INFINITY);
        else if (step == 8) {
            std::string large(65537, 'x');
            snapshot.set("Text", ByteString(large.data(), large.size()));
        }

        return Optional<Snapshot> { move(snapshot) };
    }

    void stop() override
    {
        if (m_feed->stopped)
            return;
        m_feed->stopped = true;
        m_feed->steps.clear();
        ++stopped;
    }

private:
    std::shared_ptr<Feed> m_feed;
};
}

int main(int argc, char** argv)
{
    bool expected_failure = false;
    bool leave_active = false;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--expect-failure")
            expected_failure = true;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--leave-active")
            leave_active = true;
    owner_thread = std::this_thread::get_id();
    JS::VM* owner_vm = nullptr;
    IdleCollectionState collection_state;
    auto& idle_collection = collection_state.capability;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& loop) -> JS::ThrowCompletionOr<void> {
        owner_vm = &vm;
        GC::Root native { create_host(
            realm, loop, [](ByteString, Vector<Definition>) -> ErrorOr<NonnullOwnPtr<Provider>> {
                int selected = mode;
                mode = 0;
                if (selected == 1)
                    return Error::from_errno(ENOTSUP);
                if (selected == 2)
                    return Error::from_string_literal("Controlled provider startup failure");
                auto feed = std::make_shared<Feed>();
                feeds.push_back(feed);
                return adopt_own<Provider>(*new ControlledProvider(std::move(feed)));
            }) };
        native->define_native_function(
            realm, "control"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                mode = vm.argument(0).as_i32();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "feed"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto index = vm.argument(0).as_i32();
                VERIFY(index >= 0 && static_cast<size_t>(index) < feeds.size());
                feeds[index]->steps.push_back(vm.argument(1).as_i32());
                return JS::js_undefined();
            },
            2, JS::default_attributes);
        native->define_native_function(
            realm, "stats"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                JsonObject stats;
                stats.set("created"sv, created);
                stats.set("stopped"sv, stopped);
                stats.set("destroyed"sv, destroyed);
                stats.set("polled"sv, polled);
                auto text = JsonValue(move(stats)).serialized();
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(text.bytes_as_string_view()));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        native->define_native_function(realm, "collectAtIdle"_utf16,
            GC::create_function(vm.heap(),
                GC::captures(Mite::native_reference(collection_state), realm),
                [](auto& state, auto& realm, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& idle_collection = state.get().capability;
                    if (!idle_collection)
                        idle_collection = TRY(JS::new_promise_capability(
                            vm, realm.intrinsics().promise_constructor().ptr()));
                    return idle_collection->promise();
                }),
            0, JS::default_attributes);
        native->define_native_function(
            realm, "track"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                VERIFY(vm.argument(0).is_function());
                tracked.emplace_back(vm.argument(0).as_function());
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        // Registered after the production hook: provider stop/destruction and callback
        // revocation must precede collection, while the realm/VM still exist.
        loop.on_shutdown([&vm] {
            if (created != stopped || created != destroyed)
                passed = false;
            vm.heap().collect_garbage();
            for (auto const& callback : tracked)
                if (callback)
                    passed = false;
            tracked.clear();
        });
        return modules.register_host_module("policy-host-test", *native,
            { "start", "stop", "control", "feed", "stats", "collect", "collectAtIdle", "track" });
    };

    hooks.drive = [&](Mite::EventLoop& loop) {
        do {
            if (loop.pump_once())
                break;
            if (leave_active && created && polled)
                break;
            if (idle_collection) {
                owner_vm->heap().collect_garbage();
                GC::Root resolving { idle_collection->resolve() };
                idle_collection = nullptr;
                loop.dispatch([&] { return JS::call(*owner_vm, *resolving, JS::js_undefined()); });
            }

            if (idle_collection || loop.is_alive())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (idle_collection || loop.is_alive());
        return loop.status();
    };

    hooks.shutdown = [&] {
        idle_collection = nullptr;
        owner_vm = nullptr;
    };

    int status = Mite::run(argc, argv, hooks);
    if (status != (expected_failure ? 1 : 0) || !passed || created != stopped
        || created != destroyed) {
        std::fprintf(stderr,
            "FAIL policy Host teardown status=%d created=%d stopped=%d destroyed=%d polled=%d\n",
            status, created, stopped, destroyed, polled);
        return 1;
    }

    std::printf("PASS exact policy Host owner timers provider/root teardown before VM "
                "runtimeStatus=%d; no OS policy access\n",
        status);
    return 0;
}
