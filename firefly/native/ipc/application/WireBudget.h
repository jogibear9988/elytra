/** Native wire reservations follow payloads through deferred world entry and teardown. */
#pragma once
#include <AK/RefCounted.h>
#include <AK/RefPtr.h>

namespace Firefly {
class IpcWireReservation;

class IpcWireBudget final : public RefCounted<IpcWireBudget> {
public:
    RefPtr<IpcWireReservation> reserve(size_t bytes);

private:
    friend class IpcWireReservation;

    static size_t& process_bytes()
    {
        static size_t retained = 0;
        return retained;
    }

    size_t m_bytes { 0 };
};

class IpcWireReservation final : public RefCounted<IpcWireReservation> {
public:
    ~IpcWireReservation()
    {
        m_budget->m_bytes -= m_bytes;
        IpcWireBudget::process_bytes() -= m_bytes;
    }

private:
    friend class IpcWireBudget;

    IpcWireReservation(IpcWireBudget& budget, size_t bytes)
        : m_budget(budget)
        , m_bytes(bytes)
    {
        budget.m_bytes += bytes;
        IpcWireBudget::process_bytes() += bytes;
    }

    NonnullRefPtr<IpcWireBudget> m_budget;
    size_t m_bytes;
};

inline RefPtr<IpcWireReservation> IpcWireBudget::reserve(size_t bytes)
{
    constexpr size_t document_limit = 16 * 1024 * 1024;
    constexpr size_t process_limit = 64 * 1024 * 1024;
    if (bytes > document_limit - m_bytes || bytes > process_limit - process_bytes())
        return nullptr;
    return adopt_ref(*new IpcWireReservation(*this, bytes));
}
}
