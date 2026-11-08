/** Exact production Host and real LibJS factory; only OS ownership is controlled. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/power-save-blocker/Host.h"
#include "native/power-save-blocker/Provider.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <cstdio>
#include <set>
#include <vector>

namespace {
struct Counters {
    int created {}, destroyed {}, starts {}, stops {}, queries {}, shutdowns {}, owned {};
    int mode {};
} counts;

class ControlledProvider final : public Firefly::PowerSaveBlocker::Provider {
public:
    ControlledProvider()
    {
        ++counts.created;
    }

    ~ControlledProvider() override
    {
        shutdown();
        ++counts.destroyed;
    }

    Firefly::PowerSaveBlocker::Result start(Firefly::PowerSaveBlocker::Type) override
    {
        ++counts.starts;
        if (counts.mode == 1) {
            counts.mode = 0;
            return { .code = "ERR_CONTROLLED_NATIVE", .message = "Native failure: Grüße 日本語" };
        }

        auto id = m_next++;
        m_owned.insert(id);
        ++counts.owned;
        if (counts.mode == 2) {
            counts.mode = 0;
            return { -1, false, {}, {} };
        }

        return { id, false, {}, {} };
    }

    Firefly::PowerSaveBlocker::Result stop(int id) override
    {
        ++counts.stops;
        if (counts.mode == 3) {
            counts.mode = 0;
            return { .code = "ERR_CONTROLLED_NATIVE", .message = "Native failure: Grüße 日本語" };
        }

        bool removed = m_owned.erase(id) != 0;
        counts.owned -= removed;
        return { -1, removed, {}, {} };
    }

    Firefly::PowerSaveBlocker::Result is_started(int id) const override
    {
        ++counts.queries;
        if (counts.mode == 4) {
            counts.mode = 0;
            return { .code = "ERR_CONTROLLED_NATIVE", .message = "Native failure: Grüße 日本語" };
        }

        return { -1, m_owned.contains(id), {}, {} };
    }

    void shutdown() override
    {
        if (m_stopped)
            return;
        m_stopped = true;
        ++counts.shutdowns;
        counts.owned -= static_cast<int>(m_owned.size());
        m_owned.clear();
    }

private:
    int m_next {};
    bool m_stopped {};
    std::set<int> m_owned;
};
}

namespace Firefly::PowerSaveBlocker {
std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<ControlledProvider>();
}
}

int main(int argc, char** argv)
{
    std::vector<std::shared_ptr<Firefly::PowerSaveBlocker::Controller>> controllers;
    bool had_outstanding_at_shutdown = false;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        for (auto const* name : { "power-save-test", "power-save-teardown-test" }) {
            GC::Root native { JS::Object::create(realm, nullptr) };
            controllers.push_back(Firefly::PowerSaveBlocker::install_host(realm, *native));
            native->define_native_function(
                realm, "control"_utf16,
                +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    counts.mode = vm.argument(0).as_i32();
                    return JS::js_undefined();
                },
                1, JS::default_attributes);
            native->define_native_function(
                realm, "counts"_utf16,
                +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto text = std::to_string(counts.created) + "," + std::to_string(counts.starts)
                        + "," + std::to_string(counts.stops) + "," + std::to_string(counts.queries)
                        + "," + std::to_string(counts.shutdowns) + ","
                        + std::to_string(counts.owned);
                    return JS::PrimitiveString::create(
                        vm, Utf16String::from_utf8(StringView(text.data(), text.size())));
                },
                0, JS::default_attributes);
            native->define_native_function(
                realm, "collect"_utf16,
                +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    vm.heap().collect_garbage();
                    return JS::js_undefined();
                },
                0, JS::default_attributes);
            TRY(modules.register_host_module(name, *native,
                { "powerSaveBlockerStart", "powerSaveBlockerStop", "powerSaveBlockerIsStarted",
                    "powerSaveBlockerShutdown", "control", "counts", "collect" }));
        }

        return {};
    };

    hooks.shutdown = [&] {
        had_outstanding_at_shutdown = counts.owned == 1;
        // Destruction, rather than an explicit JS shutdown, must revoke the second
        // service even though native JS function closures still retain its State.
        controllers.clear();
    };

    int status = Mite::run(argc, argv, hooks);
    if (status != 0)
        return status;
    if (!had_outstanding_at_shutdown || counts.created != 2 || counts.destroyed != 2
        || counts.shutdowns != 2 || counts.owned != 0) {
        std::fprintf(stderr,
            "FAIL controller/VM teardown ownership: created=%d destroyed=%d shutdown=%d owned=%d "
            "pending=%d\n",
            counts.created, counts.destroyed, counts.shutdowns, counts.owned,
            had_outstanding_at_shutdown);
        return 1;
    }

    std::puts("PASS exact native Host controller/VM disposal; no OS assertions requested");
    return 0;
}
