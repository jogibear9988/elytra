/** Parse switches without consuming positional values; '--' terminates switch scanning. */
#include "State.h"
#include <algorithm>

namespace Firefly::CommandLine {
State::State(int argc, char** argv, Apply apply)
    : m_apply(std::move(apply))
{
    // StartupOptions has already removed executable-owned options. Skip exe/entry.
    for (int index = 2; index < argc; ++index) {
        std::string_view token(argv[index]);
        if (token == "--")
            break;
        auto prefix = token.starts_with("--") ? 2 : token.starts_with('-') ? 1 : 0;
        if (!prefix || token.size() == static_cast<size_t>(prefix))
            continue;
        token.remove_prefix(prefix);
        auto equal = token.find('=');
        auto name = token.substr(0, equal);
        if (!valid_name(name))
            continue; // Noncanonical app arguments remain untouched in process.argv.
        auto value = equal == token.npos ? std::string_view {} : token.substr(equal + 1);
        m_switches[std::string(name)] = std::string(value);
        if (!within_budget()) {
            m_error = Failure { "ERR_OUT_OF_RANGE", "Command line snapshot exceeds switch budget" };
            return;
        }
    }
}

bool State::valid_name(std::string_view name)
{
    return !name.empty() && name.size() <= 128 && name.front() != '-'
        && std::all_of(name.begin(), name.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
           });
}

bool State::supported(std::string_view name)
{
    return name == "lang" || name == "force-cpu-painting";
}

bool State::within_budget() const
{
    size_t total = 0;
    for (auto const& [name, value] : m_switches) {
        if (value.size() > 32768 || value.find('\0') != value.npos)
            return false;
        total += name.size() + value.size();
    }

    return m_switches.size() <= 256 && total <= 65536;
}

bool State::has(std::string_view name) const
{
    return m_switches.contains(name);
}

std::string State::value(std::string_view name) const
{
    auto found = m_switches.find(name);
    return found == m_switches.end() ? std::string {} : found->second;
}

std::optional<Failure> State::change(std::string_view name, std::optional<std::string> value)
{
    if (!valid_name(name))
        return Failure { "ERR_INVALID_ARG_VALUE", "Invalid switch name" };
    if (m_sealed)
        return Failure { "ERR_FIREFLY_COMMAND_LINE_TOO_LATE",
            "Engine switches must be changed before readiness" };

    if (!supported(name))
        return Failure { "ERR_FIREFLY_UNSUPPORTED_API",
            "Unsupported engine switch: " + std::string(name) };
    auto previous = m_switches;
    if (value.has_value())
        m_switches[std::string(name)] = *value;
    else
        m_switches.erase(std::string(name));
    auto rollback = [&](Failure error) -> std::optional<Failure> {
        m_switches = std::move(previous);
        return error;
    };

    if (!within_budget())
        return rollback({ "ERR_OUT_OF_RANGE", "Command line snapshot exceeds switch budget" });
    if (auto error = m_apply(name, value, false); error.has_value())
        return rollback(*error);
    return {};
}

std::optional<Failure> State::apply_initial()
{
    if (m_error.has_value())
        return m_error;
    for (auto const& [name, value] : m_switches) {
        if (supported(name)) {
            if (auto error = m_apply(name, value, true); error.has_value())
                return error;
        }
    }

    return {};
}
}
