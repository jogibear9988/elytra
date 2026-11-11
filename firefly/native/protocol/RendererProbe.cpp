/** Configure only this diagnostic parent, then reuse the real headless renderer probe. */
#include "OriginPolicy.h"
#include <AK/Array.h>
#include <LibMain/Main.h>

namespace {
char const* chosen_helper = FIREFLY_PROTOCOL_DIAGNOSTIC_DIRECTORY;
}

ErrorOr<int> firefly_base_renderer_main(Main::Arguments);

// Reuse its actual process ownership, sandboxed helper, page loading and text
// observation. No protocol test options enter the ordinary renderer probe.
#define FIREFLY_LADYBIRD_DIRECTORY chosen_helper
#define FIREFLY_DIAGNOSTIC_ARGUMENTS_OWNED
#define ladybird_main firefly_base_renderer_main
#include "../../renderer/main.cpp"
#undef ladybird_main
#undef FIREFLY_LADYBIRD_DIRECTORY
#undef FIREFLY_DIAGNOSTIC_ARGUMENTS_OWNED

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
    Vector<StringView> filtered;
    filtered.append(arguments.strings.first());
    bool configure = false;
    for (auto argument : arguments.strings.slice(1)) {
        if (argument == "--protocol-policy"sv) {
            configure = true;
            continue;
        }
#ifdef FIREFLY_PROTOCOL_RUNTIME_DIRECTORY
        if (argument == "--protocol-runtime-helper"sv) {
            chosen_helper = FIREFLY_PROTOCOL_RUNTIME_DIRECTORY;
            continue;
        }
#endif
        filtered.append(argument);
    }

    if (configure) {
        Array schemes { Firefly::Protocols::OriginScheme { "app"_string, true, true } };
        TRY(Firefly::Protocols::install_origin_schemes(schemes));
    }

    arguments.strings = filtered.span();
    return firefly_base_renderer_main(arguments);
}
