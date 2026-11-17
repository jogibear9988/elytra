/** Probe-only read-only WFP observation. Never installs policy or loopback exemptions. */
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fwpmu.h>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace SandboxNetworkProbe {
inline ULONGLONG timestamp(FILETIME value)
{
    return (static_cast<ULONGLONG>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

struct Identity {
    PSID package;
    FWP_BYTE_BLOB const& application;
    UINT16 remote_port;
    ULONGLONG begin;
    ULONGLONG end;
};

// Header3 has no PID/birth field. These predicates bind the event to a unique
// fresh profile, exact OS application identifier and actual socket tuple; the
// owner separately retains and verifies the real child process HANDLE/birth.
// https://learn.microsoft.com/windows/win32/api/fwpmtypes/ns-fwpmtypes-fwpm_net_event_header3
inline bool matches(FWPM_NET_EVENT3 const& event, Identity const& identity)
{
    auto const& header = event.header;
    constexpr UINT32 required = FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET
        | FWPM_NET_EVENT_FLAG_IP_VERSION_SET | FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET
        | FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET | FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET
        | FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET | FWPM_NET_EVENT_FLAG_APP_ID_SET
        | FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET;
    if (event.type != FWPM_NET_EVENT_TYPE_CAPABILITY_DROP || !event.capabilityDrop
        || (header.flags & required) != required || header.ipVersion != FWP_IP_VERSION_V4
        || header.ipProtocol != IPPROTO_TCP || header.localAddrV4 != INADDR_LOOPBACK
        || header.remoteAddrV4 != INADDR_LOOPBACK || header.localPort == 0
        || header.remotePort != identity.remote_port || !header.packageSid
        || !IsValidSid(header.packageSid) || !EqualSid(header.packageSid, identity.package)
        || header.appId.size != identity.application.size || !header.appId.data
        || std::memcmp(header.appId.data, identity.application.data, header.appId.size) != 0
        || timestamp(header.timeStamp) < identity.begin
        || timestamp(header.timeStamp) > identity.end)
        return false;
    auto const& drop = *event.capabilityDrop;
    // Exact SDK enum, not the unrelated NETISO_ERROR_TYPE numeric values.
    // https://learn.microsoft.com/windows/win32/api/fwpmtypes/ns-fwpmtypes-fwpm_net_event_capability_drop0
    return drop.isLoopback == TRUE && drop.filterId != 0
        && (drop.networkCapabilityId == FWPM_APPC_NETWORK_CAPABILITY_INTERNET_CLIENT
            || drop.networkCapabilityId == FWPM_APPC_NETWORK_CAPABILITY_INTERNET_CLIENT_SERVER
            || drop.networkCapabilityId == FWPM_APPC_NETWORK_CAPABILITY_INTERNET_PRIVATE_NETWORK);
}

struct Record {
    UINT16 local_port;
    UINT64 filter;
    UINT32 capability;
    ULONGLONG time;
};

class Observer {
public:
    Observer(wchar_t const* image, PSID package, UINT16 port, HANDLE child)
        : m_port(port)
        , m_child(child)
    {
        FILETIME exit {}, kernel {}, user {};
        check(GetProcessTimes(child, &m_birth, &exit, &kernel, &user), "WFP child birth");

        m_pid = GetProcessId(child);

        check(m_pid != 0 && IsValidSid(package), "WFP child identity");

        m_package.resize(GetLengthSid(package));

        check(CopySid(static_cast<DWORD>(m_package.size()), m_package.data(), package),
            "WFP package copy");

        try {
            FWPM_SESSION0 session {};
            session.flags = FWPM_SESSION_FLAG_DYNAMIC;
            check(FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &m_engine)
                    == ERROR_SUCCESS,
                "WFP engine open (read/subscribe rights required)");

            FWP_VALUE0* value = nullptr;
            auto status = FwpmEngineGetOption0(m_engine, FWPM_ENGINE_COLLECT_NET_EVENTS, &value);

            bool enabled = status == ERROR_SUCCESS && value && value->type == FWP_UINT32
                && value->uint32 == 1;
            if (value)
                FwpmFreeMemory0(reinterpret_cast<void**>(&value));

            check(enabled, "WFP collection disabled/unreadable; no global setting changed");

            check(FwpmGetAppIdFromFileName0(image, &m_application) == ERROR_SUCCESS,
                "WFP exact worker application identity");

            check(m_application && m_application->size > 0 && m_application->data,
                "WFP nonempty application identity");

            std::array<FWPM_FILTER_CONDITION0, 3> conditions {};
            conditions[0].fieldKey = FWPM_CONDITION_ALE_APP_ID;
            conditions[0].matchType = FWP_MATCH_EQUAL;
            conditions[0].conditionValue.type = FWP_BYTE_BLOB_TYPE;
            conditions[0].conditionValue.byteBlob = m_application;
            conditions[1].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
            conditions[1].matchType = FWP_MATCH_EQUAL;
            conditions[1].conditionValue.type = FWP_UINT8;
            conditions[1].conditionValue.uint8 = IPPROTO_TCP;
            conditions[2].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
            conditions[2].matchType = FWP_MATCH_EQUAL;
            conditions[2].conditionValue.type = FWP_UINT16;
            conditions[2].conditionValue.uint16 = port;
            FWPM_NET_EVENT_ENUM_TEMPLATE0 selection {};
            selection.numFilterConditions = static_cast<UINT32>(conditions.size());

            selection.filterCondition = conditions.data();

            FWPM_NET_EVENT_SUBSCRIPTION0 subscription {};
            subscription.enumTemplate = &selection;
            check(FwpmNetEventSubscribe2(m_engine, &subscription, receive, this, &m_subscription)
                    == ERROR_SUCCESS,
                "WFP capability-drop subscription unavailable");
        } catch (...) {
            close();

            throw;
        }
    }

    ~Observer()
    {
        close();
    }

    Observer(Observer const&) = delete;
    Observer& operator=(Observer const&) = delete;

    void arm()
    {
        std::lock_guard lock(m_mutex);

        FILETIME now {};
        GetSystemTimeAsFileTime(&now);

        m_begin = timestamp(now);

        check(m_begin >= timestamp(m_birth), "WFP window follows child birth");
    }

    void finish(FILETIME exit)
    {
        {
            std::unique_lock lock(m_mutex);

            m_end = timestamp(exit);

            check(m_begin != 0 && m_end >= m_begin, "WFP exact child lifetime window");
            // Kernel notification delivery may lag the event timestamp. Retain
            // only events inside the closed child window, with a fixed wait.
            m_received.wait_for(lock, std::chrono::seconds(2), [&] { return m_count != 0; });
        }
        // Unsubscribe waits for callbacks. Never hold their mutex across it.
        // https://learn.microsoft.com/windows/win32/api/fwpmu/nf-fwpmu-fwpmneteventunsubscribe0
        close();
    }

    std::vector<Record> records()
    {
        std::lock_guard lock(m_mutex);

        check(!m_overflow, "WFP matching-event budget exceeded");

        FILETIME birth {}, exit {}, kernel {}, user {};
        check(GetProcessId(m_child) == m_pid
                && GetProcessTimes(m_child, &birth, &exit, &kernel, &user)
                && timestamp(birth) == timestamp(m_birth),
            "WFP retained child identity");

        std::vector<Record> result;
        for (size_t i = 0; i < m_count; ++i) {
            if (m_records[i].time <= m_end)
                result.push_back(m_records[i]);
        }

        return result;
    }

private:
    static void check(bool value, char const* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    static void CALLBACK receive(void* context, FWPM_NET_EVENT3 const* event)
    {
        auto& self = *static_cast<Observer*>(context);

        std::lock_guard lock(self.m_mutex);

        if (!event || self.m_begin == 0
            || !matches(*event,
                { self.m_package.data(), *self.m_application, self.m_port, self.m_begin,
                    self.m_end }))
            return;
        if (self.m_count == self.m_records.size()) {
            self.m_overflow = true;
            return;
        }

        auto const& drop = *event->capabilityDrop;
        self.m_records[self.m_count++] = { event->header.localPort, drop.filterId,
            static_cast<UINT32>(drop.networkCapabilityId), timestamp(event->header.timeStamp) };
        self.m_received.notify_one();
    }

    void close() noexcept
    {
        // A failed cancellation cannot leave a callback borrowing destroyed
        // storage. Fail this diagnostic process closed instead of returning.
        if (m_subscription) {
            if (FwpmNetEventUnsubscribe0(m_engine, m_subscription) != ERROR_SUCCESS)
                std::terminate();

            m_subscription = nullptr;
        }

        if (m_engine) {
            if (FwpmEngineClose0(m_engine) != ERROR_SUCCESS)
                std::terminate();

            m_engine = nullptr;
        }

        if (m_application)
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_application));
    }

    HANDLE m_engine { nullptr };
    HANDLE m_subscription { nullptr };
    FWP_BYTE_BLOB* m_application { nullptr };
    std::vector<BYTE> m_package;
    UINT16 m_port;
    HANDLE m_child; // Borrowed; the exact Child owner outlives this observer.
    DWORD m_pid { 0 };
    FILETIME m_birth {};
    std::mutex m_mutex;
    std::condition_variable m_received;
    ULONGLONG m_begin { 0 };
    ULONGLONG m_end { ~ULONGLONG { 0 } };
    std::array<Record, 8> m_records {};
    size_t m_count { 0 };
    bool m_overflow { false };
};
}
