/** Trusted per-window process data, independent of the actual renderer control command line. */
#pragma once
#include <AK/Error.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>

namespace Firefly::PreloadProcess {
constexpr size_t max_arguments = 64;
constexpr size_t max_argument_bytes = 16384;
constexpr size_t max_metadata_bytes = 65536;
constexpr size_t max_environment_entries = 1024;

struct Configuration {
    JsonArray arguments;
    JsonObject environment;
    size_t bytes { 0 };
};

ErrorOr<Configuration> decode(JsonValue const&);

JsonObject encode(Configuration const&);
}
