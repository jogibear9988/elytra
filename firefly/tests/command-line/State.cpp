/** Independent native parser/transaction/limits test; actual engine effects use the public gate. */
#include "native/command-line/State.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using Firefly::CommandLine::Failure;
using Firefly::CommandLine::State;

namespace {
void check(bool condition, char const* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    std::vector<std::string> storage { "firefly", "app.mjs", "--one=first", "position",
        "--one=last=x", "-flag", "--empty=", "--", "--ignored=yes" };
    std::vector<char*> args;
    for (auto& value : storage)
        args.push_back(value.data());
    int calls = 0;
    std::optional<std::string> applied;
    State state(
        args.size(), args.data(), [&](auto, auto const& value, bool) -> std::optional<Failure> {
            if (value == "invalid")
                return Failure { "ERR_INVALID_ARG_VALUE", "injected refusal" };
            applied = value;
            ++calls;
            return {};
        });
    check(!state.error() && state.has("one") && state.value("one") == "last=x",
        "last switch or '=' parsing");
    check(state.has("flag") && state.value("flag").empty() && state.has("empty")
            && !state.has("ignored"),
        "empty/terminator parsing");
    check(!state.apply_initial() && calls == 0, "unsupported opaque app arguments were applied");
    check(!state.change("lang", "de-DE") && applied == "de-DE" && state.value("lang") == "de-DE",
        "native apply failed");
    check(state.change("lang", "invalid").has_value() && state.value("lang") == "de-DE",
        "failed apply changed snapshot");
    check(
        state.change("lang", std::string(32769, 'x')).has_value() && state.value("lang") == "de-DE",
        "value budget not atomic");
    check(state.change("enable-features", "Anything")->code == "ERR_FIREFLY_UNSUPPORTED_API",
        "inert feature switch accepted");
    check(!state.change("lang", {}) && !state.has("lang") && !applied.has_value(),
        "removal did not restore native policy");
    check(
        state.change("--lang", "en")->code == "ERR_INVALID_ARG_VALUE", "name validation bypassed");
    state.seal();
    check(state.change("lang", "en")->code == "ERR_FIREFLY_COMMAND_LINE_TOO_LATE",
        "late mutation accepted");
    check(state.has("one"), "sealing disabled observation");
    storage = { "firefly", "app.mjs", "--lang=fr-FR", "--force-cpu-painting" };
    args.clear();
    for (auto& value : storage)
        args.push_back(value.data());
    State initial(args.size(), args.data(), [&](auto, auto const&, bool) -> std::optional<Failure> {
        ++calls;
        return {};
    });
    int before = calls;
    check(!initial.apply_initial() && calls == before + 2, "supported argv policy not applied");
    storage = { "firefly", "app.mjs" };
    for (int index = 0; index < 257; ++index)
        storage.push_back("--switch-" + std::to_string(index));
    args.clear();
    for (auto& value : storage)
        args.push_back(value.data());
    State excessive(args.size(), args.data(), {});
    check(excessive.error().has_value() && excessive.error()->code == "ERR_OUT_OF_RANGE",
        "initial count bound ignored");
    std::cout << "PASS native command-line parsing, snapshots, actual apply boundary, rollback, "
                 "budgets and sealing\n";
}
