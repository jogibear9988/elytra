/**
 * Bounded data-URL module decoding. This uses only neutral URL/AK facilities;
 * no filesystem paths, renderer fetch services or JavaScript coercion participate.
 */
#pragma once

#include <LibURL/URL.h>
#include <string>

namespace Mite::DataModule {
inline constexpr size_t url_limit = 8 * 1024 * 1024;
inline constexpr size_t source_limit = 4 * 1024 * 1024;
inline constexpr size_t retained_limit = 32 * 1024 * 1024;
inline constexpr size_t module_limit = 1024;

enum class Format { JavaScript, Json };
enum class Failure { None, Limit, InvalidURL, UnknownFormat, UnsupportedWasm };

struct Result {
    Failure failure { Failure::None };
    Format format { Format::JavaScript };
    std::string source;
};

// Format inspection does not allocate/decode the source body. Loading performs
// the WHATWG percent/forgiving-base64 decode with the same URL/format validation.
Result inspect(URL::URL const&, bool decode_source = false);
}
