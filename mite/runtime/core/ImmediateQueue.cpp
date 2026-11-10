/**
 * Immediates run after I/O polling, in a snapshot of the FIFO for this iteration.
 * A temporary idle handle prevents blocking while calls await that phase; it is
 * stopped when the queue drains. Only referenced calls keep the process alive.
 */
#include "core/ImmediateQueue.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Mite {
struct ImmediateQueue::Impl {
    struct Call {
        GC::Root<JS::FunctionObject> callback;
        GC::Root<JS::Object> receiver;
        GC::RootVector<JS::Value> arguments;
        bool referenced { true };
    };

    EventLoop& owner;
    JS::VM& vm;
    uv_check_t* check { new uv_check_t {} };
    uv_idle_t* idle { new uv_idle_t {} };
    std::unordered_map<uint64_t, std::unique_ptr<Call>> calls;
    std::vector<uint64_t> queue;
    uint64_t next_id { 1 };
    size_t referenced { 0 };

    Impl(EventLoop& owner, JS::VM& vm)
        : owner(owner)
        , vm(vm)
    {
        VERIFY(uv_check_init(&owner.native_loop(), check) == 0);
        VERIFY(uv_idle_init(&owner.native_loop(), idle) == 0);
        check->data = this;
        uv_unref(reinterpret_cast<uv_handle_t*>(check));
        uv_unref(reinterpret_cast<uv_handle_t*>(idle));
    }

    ~Impl()
    {
        // Native close callbacks only free handle storage; no callback may retain
        // this queue or its JS roots during the EventLoop shutdown drain.
        calls.clear();
        uv_check_stop(check);
        uv_idle_stop(idle);
        uv_close(reinterpret_cast<uv_handle_t*>(check),
            [](uv_handle_t* handle) { delete reinterpret_cast<uv_check_t*>(handle); });
        uv_close(reinterpret_cast<uv_handle_t*>(idle),
            [](uv_handle_t* handle) { delete reinterpret_cast<uv_idle_t*>(handle); });
    }

    void update()
    {
        if (calls.empty()) {
            queue.clear();
            uv_check_stop(check);
            uv_idle_stop(idle);
        } else {
            VERIFY(uv_idle_start(idle, [](uv_idle_t*) { }) == 0);
            VERIFY(uv_check_start(check, [](uv_check_t* handle) {
                static_cast<Impl*>(handle->data)->drain();
            }) == 0);
        }

        if (referenced)
            uv_ref(reinterpret_cast<uv_handle_t*>(check));
        else
            uv_unref(reinterpret_cast<uv_handle_t*>(check));
    }

    void cancel(uint64_t id)
    {
        auto found = calls.find(id);
        if (found == calls.end())
            return;
        referenced -= found->second->referenced;
        calls.erase(found);
        // Repeated schedule/cancel while another call stays live must not leave
        // an unbounded native ID list before JavaScript yields.
        if (queue.size() > calls.size() * 2 + 1024)
            std::erase_if(queue, [&](auto key) { return !calls.contains(key); });
        update();
    }

    void drain()
    {
        auto batch = std::exchange(queue, {});
        for (auto id : batch) {
            if (owner.status())
                break;
            auto found = calls.find(id);
            if (found == calls.end())
                continue;
            auto call = std::move(found->second);
            referenced -= call->referenced;
            calls.erase(found);
            update();
            // Keep all callback/receiver/argument roots through its checkpoint.
            // New immediates join the next iteration; ticks/jobs run between calls.
            owner.dispatch([&] {
                return JS::call(vm, *call->callback, JS::Value(call->receiver.ptr()),
                    ReadonlySpan<JS::Value> { call->arguments.data(), call->arguments.size() });
            });
        }

        update();
    }

    JS::ThrowCompletionOr<JS::Value> schedule()
    {
        if (!vm.argument(0).is_function() || !vm.argument(1).is_object())
            return vm.throw_completion<JS::TypeError>("Invalid internal immediate arguments"_utf16);
        if (next_id >= 9007199254740992ULL)
            return vm.throw_completion<JS::RangeError>("Immediate ID range exhausted"_utf16);
        auto id = next_id++;
        auto call = std::make_unique<Call>(Call { GC::Root { vm.argument(0).as_function() },
            GC::Root { vm.argument(1).as_object() }, {} });
        for (size_t i = 2; i < vm.argument_count(); ++i)
            call->arguments.append(vm.argument(i));
        calls.emplace(id, std::move(call));
        queue.push_back(id);
        ++referenced;
        update();
        return JS::Value(static_cast<double>(id));
    }

    JS::Value control()
    {
        auto value = vm.argument(0);
        if (!value.is_number() || value.as_double() < 1 || value.as_double() >= 9007199254740992.0
            || std::trunc(value.as_double()) != value.as_double() || !vm.argument(1).is_int32())
            return JS::Value(false);
        auto id = static_cast<uint64_t>(value.as_double());
        auto found = calls.find(id);
        if (found == calls.end())
            return JS::Value(false);
        auto operation = vm.argument(1).as_i32();
        if (operation == 3) {
            cancel(id);
            return JS::Value(false);
        }

        if (operation == 1 || operation == 2) {
            referenced -= found->second->referenced;
            found->second->referenced = operation == 1;
            referenced += found->second->referenced;
            update();
        }

        return JS::Value(found->second->referenced);
    }
};

ImmediateQueue::ImmediateQueue(EventLoop& owner, JS::VM& vm)
    : m_impl(std::make_unique<Impl>(owner, vm))
{
}

ImmediateQueue::~ImmediateQueue()
{
    revoke_native_captures();
}

void ImmediateQueue::visit_capture_edges(GC::Cell::Visitor& visitor) const
{
    if (!m_impl)
        return;
    for (auto const& entry : m_impl->calls) {
        visitor.visit(entry.second->callback.ptr());
        visitor.visit(entry.second->receiver.ptr());
        for (auto const& value : entry.second->arguments)
            visitor.visit(value);
    }
}

void ImmediateQueue::install(JS::Realm& realm)
{
    realm.global_object().define_native_function(realm, "__miteScheduleImmediate"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM&) {
                auto* self = &captured_self.get();
                return self->m_impl->schedule();
            }),
        2, JS::default_attributes);
    realm.global_object().define_native_function(realm, "__miteControlImmediate"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();
                return self->m_impl->control();
            }),
        2, JS::default_attributes);
}
}
