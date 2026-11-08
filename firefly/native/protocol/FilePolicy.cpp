/** One owner-supplied policy snapshot survives helper launch and ordered native IPC updates. */
#include "FilePolicy.h"
#include <AK/NeverDestroyed.h>
#include <mutex>

namespace Firefly::Protocols {
namespace {
    NeverDestroyed<std::mutex> policy_mutex;
    FilePolicySnapshot policy;
}

FilePolicySnapshot file_policy_snapshot()
{
    std::lock_guard lock(policy_mutex.get());
    return policy;
}

bool apply_file_policy(u64 generation, bool active)
{
    std::lock_guard lock(policy_mutex.get());
    if ((!generation && active) || generation < policy.generation
        || (generation == policy.generation && active != policy.active))
        return false;
    policy = { generation, active };
    return true;
}

ByteString file_policy_for_helper_launch()
{
    auto snapshot = file_policy_snapshot();
    return ByteString::formatted("{}:{}", snapshot.generation, snapshot.active ? 1 : 0);
}

ErrorOr<OriginArguments> consume_file_policy_configuration(ReadonlySpan<StringView> arguments)
{
    constexpr auto prefix = "--firefly-file-policy="sv;
    OriginArguments result;
    Optional<FilePolicySnapshot> selected;
    bool positional = false;
    for (auto argument : arguments) {
        if (!positional && argument.starts_with(prefix)) {
            if (selected.has_value())
                return Error::from_string_literal("Duplicate native file interception policy");
            auto value = argument.substring_view(prefix.length());
            auto parts = value.split_view(':');
            if (value.length() > 24 || parts.size() != 2 || parts[0].is_empty()
                || (parts[1] != "0"sv && parts[1] != "1"sv))
                return Error::from_string_literal("Invalid native file interception policy");
            auto generation = parts[0].to_number<u64>();
            auto canonical
                = generation.has_value() ? ByteString::number(*generation) : ByteString {};
            if (!generation.has_value() || canonical.view() != parts[0]
                || (!*generation && parts[1] == "1"sv))
                return Error::from_string_literal("Invalid native file policy generation");
            selected = FilePolicySnapshot { *generation, parts[1] == "1"sv };
            continue;
        }

        if (!positional && argument == "--firefly-file-policy"sv)
            return Error::from_string_literal("Native file interception policy requires a value");
        if (argument == "--"sv)
            positional = true;
        TRY(result.owned.try_append(ByteString(argument)));
    }

    for (auto& value : result.owned) {
        TRY(result.strings.try_append(value.view()));
        TRY(result.argv.try_append(const_cast<char*>(value.characters())));
    }

    TRY(result.argv.try_append(nullptr));
    if (selected.has_value() && !apply_file_policy(selected->generation, selected->active))
        return Error::from_string_literal("Native file interception policy regressed");
    return result;
}
}
