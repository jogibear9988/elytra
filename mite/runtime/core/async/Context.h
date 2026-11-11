/** Traced owner-thread async frames. Only disable mutates the current shared frame, as Node24. */
#pragma once
#include "core/GCCaptures.h"
#include <LibGC/CellAllocator.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/JobCallback.h>
#include <LibJS/Runtime/VM.h>
#include <vector>

namespace Mite {
class AsyncStorage final : public GC::Cell {
    GC_CELL(AsyncStorage, GC::Cell);

    GC_DECLARE_ALLOCATOR(AsyncStorage);

public:
    // GC free-list cells require a native word beyond the Cell header.
    FlatPtr reserved {};
};

class AsyncFrame final : public GC::Cell {
    GC_CELL(AsyncFrame, GC::Cell);

    GC_DECLARE_ALLOCATOR(AsyncFrame);

public:
    struct Binding {
        GC::Ptr<AsyncStorage> storage;
        JS::Value value;
    };

    explicit AsyncFrame(Vector<Binding> bindings)
        : bindings(move(bindings))
    {
    }

    Vector<Binding> bindings;
    void visit_edges(Visitor&) override;
};

class AsyncCarrier final : public GC::Cell {
    GC_CELL(AsyncCarrier, GC::Cell);

    GC_DECLARE_ALLOCATOR(AsyncCarrier);

public:
    AsyncCarrier(GC::Ptr<AsyncFrame> frame, u64 id, u64 trigger)
        : frame(frame)
        , id(id)
        , trigger(trigger)
    {
    }

    GC::Ptr<AsyncFrame> const frame;
    u64 const id;
    u64 const trigger;
    void visit_edges(Visitor&) override;
};

class AsyncContext : public NativeCaptureOwner<AsyncContext> {
public:
    explicit AsyncContext(JS::VM&);

    ~AsyncContext();

    void visit_capture_edges(GC::Cell::Visitor&) const;

    GC::Ref<AsyncCarrier> capture();

    GC::Ptr<AsyncCarrier> current() const
    {
        return m_current.ptr();
    }

    class Scope {
    public:
        Scope(AsyncContext&, GC::Ptr<AsyncCarrier>);

        ~Scope();

    private:
        AsyncContext& m_owner;
        GC::Root<AsyncCarrier> m_previous;
    };

    GC::Ref<JS::Object> create_host(JS::Realm&);

private:
    JS::ThrowCompletionOr<GC::Ref<AsyncCarrier>> with_storage(
        AsyncStorage&, JS::Value, bool remove);

    GC::Ref<JS::Object> create_storage(JS::Realm&);

    GC::Ref<JS::Object> create_resource(JS::Realm&);

    JS::VM& m_vm;
    GC::Root<AsyncCarrier> m_current;
    u64 m_next { 1 };
    // A disabled binding can still be needed by an enclosing run() finally.
    std::vector<GC::Root<AsyncStorage>> m_restoring;
    decltype(JS::VM::host_make_job_callback) m_make;
    decltype(JS::VM::host_call_job_callback) m_call;
};
}
