/** Probe-only WFP evidence; explicit isolated-CI audit never changes network policy. */
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fwpmu.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
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

// Diagnostic admission deliberately does not require a package SID. Missing
// or unequal SID is recorded, never promoted to strict acceptance by matches().
enum class DiagnosticStage : size_t {
    Type,
    Flags,
    Protocol,
    Target,
    Application,
    Time,
    Match,
    Count
};

inline DiagnosticStage diagnostic_stage(FWPM_NET_EVENT3 const& event, Identity const& identity)
{
    auto const& header = event.header;
    bool drop = (event.type == FWPM_NET_EVENT_TYPE_CAPABILITY_DROP && event.capabilityDrop)
        || (event.type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && event.classifyDrop);

    if (!drop)
        return DiagnosticStage::Type;
    constexpr UINT32 required = FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET
        | FWPM_NET_EVENT_FLAG_IP_VERSION_SET | FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET
        | FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET | FWPM_NET_EVENT_FLAG_APP_ID_SET;
    if ((header.flags & required) != required)
        return DiagnosticStage::Flags;
    if (header.ipVersion != FWP_IP_VERSION_V4 || header.ipProtocol != IPPROTO_TCP)
        return DiagnosticStage::Protocol;
    if (header.remoteAddrV4 != INADDR_LOOPBACK || header.remotePort != identity.remote_port)
        return DiagnosticStage::Target;
    if (header.appId.size != identity.application.size || !header.appId.data
        || std::memcmp(header.appId.data, identity.application.data, header.appId.size) != 0)
        return DiagnosticStage::Application;
    if (timestamp(header.timeStamp) < identity.begin || timestamp(header.timeStamp) > identity.end)
        return DiagnosticStage::Time;
    return DiagnosticStage::Match;
}

inline bool diagnostic_matches(FWPM_NET_EVENT3 const& event, Identity const& identity)
{
    return diagnostic_stage(event, identity) == DiagnosticStage::Match;
}

struct Record {
    UINT16 local_port;
    UINT64 filter;
    UINT32 capability;
    ULONGLONG time;
};

struct Diagnostic {
    Record record;
    bool accepted;
    UINT32 type;
    UINT32 flags;
    bool sid_present;
    bool sid_equal;
    bool loopback;
    UINT16 layer;
    UINT32 direction;
    char const* local_address_class;
    bool local_port_present;
};

struct Option {
    DWORD status;
    UINT32 type;
    UINT32 value;
};

// Probe-only audit lease. No firewall rule, capability or collection policy is
// changed. The getter/setter are the actual native calls in Observer, and the
// same ownership state machine is exercised by pure native negative controls.
// Dedicated isolated CI only: equality checks cannot exclude an external
// writer racing GetOption/SetOption or restore the mask after process death.
class KeywordAudit {
public:
    template <typename Read, typename Write>
    bool apply(bool requested, Option original, Read read, Write write)
    {
        m_requested = requested;
        m_original = original;
        m_temporary = original.value | FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP;
        if (!requested)
            return true;
        if (!is_value(original, original.value))
            return false;
        if (m_temporary == original.value)
            return true; // Already enabled: no ownership or restoration claim.
        auto current = read();

        if (!is_value(current, original.value))
            return false; // Refuse observed drift; the OS API provides no atomic compare-and-swap.
        m_set_status = write(m_temporary);

        if (m_set_status != ERROR_SUCCESS)
            return false;
        m_restore_required = true; // Set succeeded even if verification fails.
        m_after_set = read();

        m_applied = is_value(m_after_set, m_temporary);

        return m_applied;
    }

    template <typename Read, typename Write> bool restore(Read read, Write write)
    {
        if (!m_restore_required)
            return true;
        if (m_restore_attempted)
            return m_restored; // Never repeat a failed write or overwrite drift.
        m_restore_attempted = true;
        m_before_restore = read();

        if (!is_value(m_before_restore, m_temporary))
            return false;
        m_restore_status = write(m_original.value);

        if (m_restore_status != ERROR_SUCCESS)
            return false;
        m_after_restore = read();

        m_restored = is_value(m_after_restore, m_original.value);

        if (m_restored)
            m_restore_required = false;
        return m_restored;
    }

    std::string report() const
    {
        std::ostringstream out;
        auto option = [&](Option value) {
            out << "{\"status\":" << value.status << ",\"type\":" << value.type
                << ",\"value\":" << value.value << '}';
        };

        out << std::boolalpha << "{\"requested\":" << m_requested << ",\"original\":";
        option(m_original);

        out << ",\"temporaryMask\":" << m_temporary << ",\"setStatus\":" << m_set_status
            << ",\"applied\":" << m_applied << ",\"afterSet\":";
        option(m_after_set);

        out << ",\"restoreRequired\":" << m_restore_required
            << ",\"restoreAttempted\":" << m_restore_attempted << ",\"beforeRestore\":";
        option(m_before_restore);

        out << ",\"restoreStatus\":" << m_restore_status << ",\"afterRestore\":";
        option(m_after_restore);

        out << ",\"restored\":" << m_restored << '}';
        return out.str();
    }

private:
    static bool is_value(Option value, UINT32 expected)
    {
        return value.status == ERROR_SUCCESS && value.type == FWP_UINT32 && value.value == expected;
    }

    bool m_requested { false };
    Option m_original {};
    UINT32 m_temporary { 0 };
    DWORD m_set_status { ERROR_NOT_READY };
    Option m_after_set { ERROR_NOT_READY, FWP_EMPTY, 0 };
    bool m_applied { false };
    bool m_restore_required { false };
    bool m_restore_attempted { false };
    Option m_before_restore { ERROR_NOT_READY, FWP_EMPTY, 0 };
    DWORD m_restore_status { ERROR_NOT_READY };
    Option m_after_restore { ERROR_NOT_READY, FWP_EMPTY, 0 };
    bool m_restored { false };
};

inline std::string guid(GUID const& value)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(8) << value.Data1 << '-' << std::setw(4)
        << value.Data2 << '-' << std::setw(4) << value.Data3 << '-';
    for (size_t i = 0; i < 8; ++i) {
        if (i == 2)
            out << '-';
        out << std::setw(2) << static_cast<unsigned>(value.Data4[i]);
    }

    return out.str();
}

class Observer {
public:
    Observer(wchar_t const* image, PSID package, UINT16 port, HANDLE child, bool audit = false)
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
            // SetOption is forbidden on dynamic sessions. Only the explicitly
            // opted-in diagnostic probe opens the ordinary audit session.
            session.flags = audit ? 0 : FWPM_SESSION_FLAG_DYNAMIC;
            check(FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &m_engine)
                    == ERROR_SUCCESS,
                "WFP engine open (read/subscribe rights required)");

            m_collection = read_option(FWPM_ENGINE_COLLECT_NET_EVENTS);
            // SDK defines CAPABILITY_DROP as keyword bit 0x4;
            // do not infer its collection state from the generic enable flag.
            // https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/shared/fwpmtypes.h
            m_keywords = read_option(FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS);

            check(m_collection.status == ERROR_SUCCESS && m_collection.type == FWP_UINT32
                    && m_collection.value == 1,
                "WFP collection disabled/unreadable; no global setting changed");

            if (!m_audit.apply(
                    audit, m_keywords,
                    [&] { return read_option(FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS); },
                    [&](UINT32 mask) { return write_keyword_mask(mask); })) {
                std::cerr << "WFP_AUDIT_APPLY_FAILURE " << m_audit.report() << '\n';
                throw std::runtime_error("WFP audit opt-in could not be applied/verified");
            }

            check(FwpmGetAppIdFromFileName0(image, &m_application) == ERROR_SUCCESS,
                "WFP exact worker application identity");

            check(m_application && m_application->size > 0 && m_application->data,
                "WFP nonempty application identity");

            // Some drop events omit the transport fields. Subscribe only to
            // our worker image so the callback can count those incomplete
            // events. Endpoint checks still gate every retained record.
            FWPM_FILTER_CONDITION0 application {};
            application.fieldKey = FWPM_CONDITION_ALE_APP_ID;
            application.matchType = FWP_MATCH_EQUAL;
            application.conditionValue.type = FWP_BYTE_BLOB_TYPE;
            application.conditionValue.byteBlob = m_application;
            FWPM_NET_EVENT_ENUM_TEMPLATE0 selection {};
            selection.numFilterConditions = 1;
            selection.filterCondition = &application;

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
            m_received.wait_for(lock, std::chrono::seconds(2), [&] {
                for (size_t i = 0; i < m_count; ++i) {
                    if (m_records[i].accepted && m_records[i].record.time <= m_end)
                        return true;
                }

                return false;
            });
        }
        // Unsubscribe waits for callbacks. Never hold their mutex across it.
        // https://learn.microsoft.com/windows/win32/api/fwpmu/nf-fwpmu-fwpmneteventunsubscribe0
        stop_subscription();

        if (!restore_audit())
            throw std::runtime_error("WFP audit ownership/restore failed; no cleanup claim");
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
            if (m_records[i].accepted && m_records[i].record.time <= m_end)
                result.push_back(m_records[i].record);
        }

        return result;
    }

    std::string diagnostics()
    {
        check(!m_subscription, "WFP metadata read only after callback drain");

        std::ostringstream out;
        out << std::boolalpha << '[';
        bool first = true;
        for (size_t i = 0; i < m_count; ++i) {
            auto const& item = m_records[i];
            if (item.record.time > m_end)
                continue;
            if (!first)
                out << ',';
            first = false;
            out << "{\"type\":" << item.type << ",\"flags\":" << item.flags
                << ",\"localPort\":" << item.record.local_port << ",\"remotePort\":" << m_port
                << ",\"timestamp\":\"" << item.record.time
                << "\",\"sidPresent\":" << item.sid_present << ",\"sidEqual\":" << item.sid_equal
                << ",\"loopback\":" << item.loopback << ",\"layer\":" << item.layer
                << ",\"direction\":" << item.direction << ",\"localAddressClass\":\""
                << item.local_address_class << "\",\"localPortPresent\":" << item.local_port_present
                << ",\"capability\":" << item.record.capability
                << ",\"strictAccepted\":" << item.accepted << ",\"filterId\":\""
                << item.record.filter << "\",\"filter\":";
            filter_metadata(out, item.record.filter);

            out << '}';
        }

        out << ']';
        return out.str();
    }

    std::string observation()
    {
        check(!m_subscription, "WFP counters read only after callback drain");

        std::ostringstream out;
        auto option = [&](Option const& value) {
            out << "{\"status\":" << value.status << ",\"type\":" << value.type
                << ",\"value\":" << value.value << '}';
        };

        out << std::boolalpha << "{\"collection\":";
        option(m_collection);

        out << ",\"keywords\":";
        option(m_keywords);

        out << ",\"capabilityDropKeyword\":" << FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP
            << ",\"capabilityDropEnabled\":"
            << (m_keywords.status == ERROR_SUCCESS && m_keywords.type == FWP_UINT32
                   && (m_keywords.value & FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP) != 0)
            << ",\"audit\":" << m_audit.report() << ",\"callbacks\":" << m_callbacks
            << ",\"null\":" << m_null << ",\"unarmed\":" << m_unarmed
            << ",\"overflow\":" << m_overflow << ",\"stages\":{";
        constexpr char const* labels[] { "type", "flags", "protocol", "target", "application",
            "time", "matched" };
        for (size_t i = 0; i < m_stages.size(); ++i) {
            if (i)
                out << ',';
            out << '"' << labels[i] << "\":" << m_stages[i];
        }

        out << "}}";
        return out.str();
    }

private:
    Option read_option(FWPM_ENGINE_OPTION option)
    {
        FWP_VALUE0* value = nullptr;
        auto status = FwpmEngineGetOption0(m_engine, option, &value);

        Option result { status, static_cast<UINT32>(FWP_EMPTY), 0 };
        if (status == ERROR_SUCCESS && value) {
            result.type = static_cast<UINT32>(value->type);

            if (value->type == FWP_UINT32)
                result.value = value->uint32;
        }

        if (value)
            FwpmFreeMemory0(reinterpret_cast<void**>(&value));

        return result;
    }

    DWORD write_keyword_mask(UINT32 mask)
    {
        // Only event-audit keywords; never COLLECT_NET_EVENTS, filters, ACLs or
        // AppContainer exemptions/capabilities. Explicit hosted-probe opt-in.
        FWP_VALUE0 value {};
        value.type = FWP_UINT32;
        value.uint32 = mask;
        return FwpmEngineSetOption0(m_engine, FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS, &value);
    }

    bool restore_audit()
    {
        bool restored
            = m_audit.restore([&] { return read_option(FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS); },
                [&](UINT32 mask) { return write_keyword_mask(mask); });

        if (!restored)
            std::cerr << "WFP_AUDIT_RESTORE_FAILURE " << m_audit.report() << '\n';
        return restored;
    }

    static void check(bool value, char const* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    static void CALLBACK receive(void* context, FWPM_NET_EVENT3 const* event)
    {
        auto& self = *static_cast<Observer*>(context);

        std::lock_guard lock(self.m_mutex);

        auto increment = [](UINT32& value) {
            if (value < 65535)
                ++value;
        };

        increment(self.m_callbacks);

        if (!event) {
            increment(self.m_null);

            return;
        }

        if (self.m_begin == 0) {
            increment(self.m_unarmed);

            return;
        }

        Identity identity { self.m_package.data(), *self.m_application, self.m_port, self.m_begin,
            self.m_end };
        auto stage = diagnostic_stage(*event, identity);

        increment(self.m_stages[static_cast<size_t>(stage)]);

        if (stage != DiagnosticStage::Match)
            return;
        if (self.m_count == self.m_records.size()) {
            self.m_overflow = true;
            return;
        }

        bool present = (event->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET)
            && event->header.packageSid && IsValidSid(event->header.packageSid);

        Diagnostic result {};
        result.accepted = matches(*event, identity);

        result.type = static_cast<UINT32>(event->type);

        result.flags = event->header.flags;
        result.sid_present = present;
        result.sid_equal = present && EqualSid(event->header.packageSid, identity.package);

        result.local_port_present = (event->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0;
        result.record.local_port = result.local_port_present ? event->header.localPort : 0;
        result.local_address_class = !(event->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
            ? "missing"
            : event->header.localAddrV4 == INADDR_ANY      ? "any"
            : event->header.localAddrV4 == INADDR_LOOPBACK ? "loopback"
                                                           : "other";
        result.record.time = timestamp(event->header.timeStamp);

        if (event->type == FWPM_NET_EVENT_TYPE_CAPABILITY_DROP) {
            auto const& drop = *event->capabilityDrop;
            result.record.filter = drop.filterId;
            result.record.capability = static_cast<UINT32>(drop.networkCapabilityId);

            result.loopback = drop.isLoopback == TRUE;
        } else {
            auto const& drop = *event->classifyDrop;
            result.record.filter = drop.filterId;
            result.layer = drop.layerId;
            result.direction = drop.msFwpDirection;
            result.loopback = drop.isLoopback == TRUE;
        }

        self.m_records[self.m_count++] = result;
        self.m_received.notify_one();
    }

    void filter_metadata(std::ostream& out, UINT64 id)
    {
        FWPM_FILTER0* filter = nullptr;
        auto status = id ? FwpmFilterGetById0(m_engine, id, &filter) : ERROR_INVALID_PARAMETER;
        auto release = [](auto* pointer) {
            if (pointer)
                FwpmFreeMemory0(reinterpret_cast<void**>(&pointer));
        };

        std::unique_ptr<FWPM_FILTER0, decltype(release)> filter_owner(filter, release);

        out << "{\"status\":" << status;
        if (status == ERROR_SUCCESS && filter) {
            out << ",\"action\":" << filter->action.type << ",\"layerKey\":\""
                << guid(filter->layerKey) << "\",\"subLayerKey\":\"" << guid(filter->subLayerKey)
                << "\",\"providerKey\":";
            if (filter->providerKey)
                out << '"' << guid(*filter->providerKey) << '"';
            else
                out << "null";
            if (filter->providerKey) {
                FWPM_PROVIDER0* provider = nullptr;
                auto provider_status
                    = FwpmProviderGetByKey0(m_engine, filter->providerKey, &provider);

                std::unique_ptr<FWPM_PROVIDER0, decltype(release)> provider_owner(
                    provider, release);

                out << ",\"providerStatus\":" << provider_status;
                if (provider_status == ERROR_SUCCESS && provider)
                    out << ",\"providerFlags\":" << provider->flags;
            }

            FWPM_SUBLAYER0* sublayer = nullptr;
            auto sublayer_status = FwpmSubLayerGetByKey0(m_engine, &filter->subLayerKey, &sublayer);

            std::unique_ptr<FWPM_SUBLAYER0, decltype(release)> sublayer_owner(sublayer, release);

            out << ",\"subLayerStatus\":" << sublayer_status;
            if (sublayer_status == ERROR_SUCCESS && sublayer) {
                out << ",\"subLayerFlags\":" << sublayer->flags << ",\"subLayerProviderKey\":";
                if (sublayer->providerKey)
                    out << '"' << guid(*sublayer->providerKey) << '"';
                else
                    out << "null";
            }

            out << ",\"conditionCount\":" << filter->numFilterConditions
                << ",\"truncated\":" << (filter->numFilterConditions > 16) << ",\"conditions\":[";
            for (UINT32 i = 0; i < filter->numFilterConditions && i < 16; ++i) {
                if (i)
                    out << ',';
                auto const& condition = filter->filterCondition[i];
                auto const& value = condition.conditionValue;
                out << "{\"field\":\"" << guid(condition.fieldKey)
                    << "\",\"match\":" << condition.matchType << ",\"valueType\":" << value.type;
                bool scalar = IsEqualGUID(condition.fieldKey, FWPM_CONDITION_FLAGS)
                    || IsEqualGUID(condition.fieldKey, FWPM_CONDITION_IP_PROTOCOL)
                    || IsEqualGUID(condition.fieldKey, FWPM_CONDITION_IP_LOCAL_PORT)
                    || IsEqualGUID(condition.fieldKey, FWPM_CONDITION_IP_REMOTE_PORT);

                switch (value.type) {
                case FWP_UINT8:
                    if (scalar)
                        out << ",\"scalar\":" << static_cast<unsigned>(value.uint8);

                    break;
                case FWP_UINT16:
                    if (scalar)
                        out << ",\"scalar\":" << value.uint16;
                    break;
                case FWP_UINT32:
                    if (scalar)
                        out << ",\"scalar\":" << value.uint32;
                    break;
                case FWP_UINT64:
                    if (scalar && value.uint64)
                        out << ",\"scalar\":\"" << *value.uint64 << '"';
                    break;
                case FWP_SID:
                    out << ",\"ownPackageSID\":"
                        << (value.sid && IsValidSid(value.sid)
                               && EqualSid(value.sid, m_package.data()));

                    break;
                default:
                    break; // Never print strings, blobs, ACLs or pointers.
                }

                out << '}';
            }

            out << ']';
        }

        out << '}';
    }

    void stop_subscription() noexcept
    {
        // A failed cancellation cannot leave a callback borrowing destroyed
        // storage. Fail this diagnostic process closed instead of returning.
        if (m_subscription) {
            if (FwpmNetEventUnsubscribe0(m_engine, m_subscription) != ERROR_SUCCESS)
                std::terminate();

            m_subscription = nullptr;
        }
    }

    void close() noexcept
    {
        stop_subscription();

        if (!restore_audit())
            std::terminate(); // Failed ownership/cleanup is never reported success.

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
    std::array<Diagnostic, 8> m_records {};
    size_t m_count { 0 };
    bool m_overflow { false };
    KeywordAudit m_audit;
    Option m_collection {};
    Option m_keywords {};
    UINT32 m_callbacks { 0 };
    UINT32 m_null { 0 };
    UINT32 m_unarmed { 0 };
    std::array<UINT32, static_cast<size_t>(DiagnosticStage::Count)> m_stages {};
};
}
