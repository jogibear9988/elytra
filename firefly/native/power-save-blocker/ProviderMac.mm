/** Synchronous IOPM assertion ownership. No Node, GUI, input monitoring or power transition APIs.
 */
#include "Provider.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>

namespace Firefly::PowerSaveBlocker {
namespace {
    Result failure(char const* code, char const* message)
    {
        return { -1, false, code, message };
    }

    class MacProvider final : public Provider {
    public:
        ~MacProvider() override
        {
            shutdown();
            // An OS release can fail. Retain at most two known assertions and
            // retry each a bounded number of times without claiming success.
            for (size_t attempt = 0; attempt < 3 && (m_current || m_rollback); ++attempt)
                release_owned();
        }

        Result start(Type type) override
        {
            if (m_stopped)
                return failure("ERR_FIREFLY_APP_EXITED", "Power blocker provider stopped");
            if (type != Type::AppSuspension && type != Type::DisplaySleep)
                return failure("ERR_INVALID_ARG_VALUE", "Unknown power blocker type");
            if (m_requests.size() >= request_limit || m_next > std::numeric_limits<int>::max())
                return failure(
                    "ERR_FIREFLY_POWER_SAVE_LIMIT", "Power blocker request budget exhausted");
            auto desired = effective();
            if (!desired || type == Type::DisplaySleep)
                desired = type;
            auto result = apply(desired);
            if (!result.code.empty())
                return result;
            auto id = static_cast<int>(m_next++);
            m_requests.emplace(id, type);
            return { id, true, {}, {} };
        }

        Result stop(int id) override
        {
            if (m_stopped)
                return {};
            auto found = m_requests.find(id);
            if (found == m_requests.end())
                return {};
            auto result = apply(effective(id));
            if (!result.code.empty())
                return result;
            m_requests.erase(found);
            return { id, true, {}, {} };
        }

        Result is_started(int id) const override
        {
            return { id, !m_stopped && m_requests.contains(id), {}, {} };
        }

        void shutdown() override
        {
            m_stopped = true;
            m_requests.clear();
            release_owned();
        }

    private:
        struct Assertion {
            IOPMAssertionID id;
            Type type;
        };

        std::optional<Type> effective(std::optional<int> excluded = {}) const
        {
            std::optional<Type> type;
            for (auto const& [id, candidate] : m_requests) {
                if (excluded && id == *excluded)
                    continue;
                if (candidate == Type::DisplaySleep)
                    return candidate;
                type = candidate;
            }

            return type;
        }

        bool release(std::optional<Assertion>& value)
        {
            if (!value)
                return true;
            if (IOPMAssertionRelease(value->id) != kIOReturnSuccess)
                return false;
            value.reset();
            return true;
        }

        void release_owned()
        {
            release(m_rollback);
            release(m_current);
        }

        Result apply(std::optional<Type> desired)
        {
            // A failed rollback owns a real assertion. Retry it before admitting
            // any further mutation/allocation, bounding ownership to two IDs.
            if (!release(m_rollback))
                return failure(
                    "ERR_FIREFLY_POWER_SAVE_RELEASE", "Power blocker rollback release failed");
            if (desired && m_current && m_current->type == *desired)
                return {};
            if (!desired) {
                if (!release(m_current))
                    return failure(
                        "ERR_FIREFLY_POWER_SAVE_RELEASE", "Power blocker release failed");
                return {};
            }

            IOPMAssertionID id = kIOPMNullAssertionID;
            auto level = *desired == Type::DisplaySleep ? kIOPMAssertionTypeNoDisplaySleep
                                                        : kIOPMAssertionTypeNoIdleSleep;
            auto status = IOPMAssertionCreateWithName(
                level, kIOPMAssertionLevelOn, CFSTR("Firefly powerSaveBlocker"), &id);
            if (status != kIOReturnSuccess || id == kIOPMNullAssertionID)
                return failure(
                    "ERR_FIREFLY_POWER_SAVE_CREATE", "Native power assertion creation failed");
            std::optional<Assertion> replacement { Assertion { id, *desired } };
            // Never remove existing protection before replacement creation succeeds.
            if (!release(m_current)) {
                if (!release(replacement))
                    m_rollback = replacement;
                return failure(
                    "ERR_FIREFLY_POWER_SAVE_RELEASE", "Power blocker replacement release failed");
            }

            m_current = replacement;
            return {};
        }

        std::unordered_map<int, Type> m_requests;
        std::optional<Assertion> m_current;
        std::optional<Assertion> m_rollback;
        uint64_t m_next {};
        bool m_stopped { false };
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<MacProvider>();
}
}
