/** Actual native launch/control policy parsing and ownership; no VM, GUI or filesystem access. */
#include "native/protocol/FilePolicy.h"
#include <AK/Array.h>
#include <cstdio>

namespace P = Firefly::Protocols;

int main()
{
    auto check = [](bool value, char const* message) {
        if (!value)
            std::fprintf(stderr, "FAIL native file interception policy: %s\n", message);
        return value;
    };

    auto snapshot = P::file_policy_snapshot();
    if (!check(!snapshot.generation && !snapshot.active
                && P::file_policy_for_helper_launch() == "0:0"sv,
            "Startup defaults"))
        return 1;
    auto initial = P::consume_file_policy_configuration(
        Array { "owned-helper"sv, "--firefly-file-policy=1:1"sv, "--ordinary=Grüße"sv, "tail"sv });
    if (!check(!initial.is_error(), "Valid native startup policy refused"))
        return 1;
    auto owned = initial.release_value();
    if (!check(owned.strings.size() == 3 && owned.argv.size() == 4
                && owned.strings[0] == "owned-helper"sv && owned.strings[1] == "--ordinary=Grüße"sv
                && owned.strings[2] == "tail"sv && owned.argv[3] == nullptr
                && owned.argv[1] == owned.owned[1].characters(),
            "Filtered argument ownership"))
        return 1;
    if (!check(P::apply_file_policy(1, true) && !P::apply_file_policy(1, false)
                && !P::apply_file_policy(0, false) && !P::apply_file_policy(0, true)
                && P::file_policy_for_helper_launch() == "1:1"sv,
            "Idempotence, equal-generation conflict and stale rollback"))
        return 1;
    for (auto invalid :
        { "--firefly-file-policy"sv, "--firefly-file-policy="sv, "--firefly-file-policy=0:1"sv,
            "--firefly-file-policy=2:2"sv, "--firefly-file-policy=2:true"sv,
            "--firefly-file-policy=+2:1"sv, "--firefly-file-policy=02:1"sv,
            "--firefly-file-policy=-1:0"sv, "--firefly-file-policy=18446744073709551616:1"sv,
            "--firefly-file-policy=2:1:0"sv, "--firefly-file-policy= 2:1"sv }) {
        auto failed = P::consume_file_policy_configuration(Array { "owned-helper"sv, invalid });
        if (!check(failed.is_error() && P::file_policy_for_helper_launch() == "1:1"sv,
                "Malformed snapshot mutated live policy"))
            return 1;
    }

    auto duplicate = P::consume_file_policy_configuration(
        Array { "owned-helper"sv, "--firefly-file-policy=2:0"sv, "--firefly-file-policy=3:1"sv });
    if (!check(duplicate.is_error() && P::file_policy_for_helper_launch() == "1:1"sv,
            "Duplicate flags partially committed"))
        return 1;
    auto positional = P::consume_file_policy_configuration(
        Array { "owned-helper"sv, "--"sv, "--firefly-file-policy=2:0"sv });
    if (!check(!positional.is_error() && positional.value().strings.size() == 3
                && positional.value().strings[2] == "--firefly-file-policy=2:0"sv
                && P::file_policy_for_helper_launch() == "1:1"sv,
            "Application arguments consumed"))
        return 1;
    if (!check(P::apply_file_policy(2, false) && !P::apply_file_policy(1, true)
                && P::file_policy_for_helper_launch() == "2:0"sv,
            "Dynamic removal or stale enable"))
        return 1;
    auto roundtrip = P::consume_file_policy_configuration(
        Array { "owned-helper"sv, "--firefly-file-policy=2:0"sv });
    if (!check(!roundtrip.is_error() && !P::file_policy_snapshot().active,
            "Idempotent helper snapshot roundtrip"))
        return 1;
    std::puts("PASS native file policy startup/argument ownership, strict parsing, atomic "
              "rejection, monotonic control and launch snapshots; no renderer authority granted");
    return 0;
}
