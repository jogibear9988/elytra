/** Parse the native launch snapshot before the renderer VM or parent connection exists. */
#include "Session.h"
#include "loading/web-request/Gate.h"

namespace Firefly::HelperWebRequest {
ErrorOr<Protocols::OriginArguments> prepare(ReadonlySpan<StringView> arguments)
{
    constexpr auto prefix = "--firefly-web-request-policy="sv;
    Protocols::OriginArguments result;
    Optional<WebRequestGate::Policy> selected;
    bool positional = false;
    for (auto argument : arguments) {
        if (!positional && argument.starts_with(prefix)) {
            auto parts = argument.substring_view(prefix.length()).split_view(':');
            if (selected.has_value() || parts.size() != 3
                || argument.length() > prefix.length() + 16)
                return Error::from_string_literal("Invalid duplicate webRequest startup policy");
            auto epoch = parts[0].to_number<u32>();
            auto canonical = epoch.has_value() ? ByteString::number(*epoch) : ByteString {};
            if (!epoch.has_value() || !*epoch || *epoch > 2147483647 || canonical.view() != parts[0]
                || !parts[1].is_one_of("0"sv, "1"sv) || !parts[2].is_one_of("0"sv, "1"sv))
                return Error::from_string_literal("Invalid webRequest startup policy");
            selected = WebRequestGate::Policy { *epoch, parts[1] == "1"sv, parts[2] == "1"sv };
            continue;
        }

        if (!positional && argument == "--firefly-web-request-policy"sv)
            return Error::from_string_literal("webRequest startup policy requires a value");
        if (argument == "--"sv)
            positional = true;
        TRY(result.owned.try_append(ByteString(argument)));
    }

    for (auto& value : result.owned) {
        TRY(result.strings.try_append(value.view()));
        TRY(result.argv.try_append(const_cast<char*>(value.characters())));
    }

    TRY(result.argv.try_append(nullptr));
    if (selected.has_value() && !WebRequestGate::configure(*selected))
        return Error::from_string_literal("webRequest startup policy regressed");
    return result;
}

}
