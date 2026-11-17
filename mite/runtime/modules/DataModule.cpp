/**
 * Implements data-URL module bodies following the WHATWG data URL processor and
 * Node v24.15.0's JavaScript/JSON MIME selection. Node's implementation is MIT:
 * https://github.com/nodejs/node/blob/v24.15.0/lib/internal/data_url.js
 * This independent adapter reuses LibURL percent decoding and AK base64.
 */
#include "modules/DataModule.h"
#include "modules/ModulePaths.h"
#include <AK/Base64.h>
#include <algorithm>
#include <string_view>

namespace Mite::DataModule {
namespace {
    bool whitespace(char ch)
    {
        return ch == '\t' || ch == '\n' || ch == '\f' || ch == '\r' || ch == ' ';
    }

    std::string_view trim(std::string_view text)
    {
        while (!text.empty() && whitespace(text.front()))
            text.remove_prefix(1);
        while (!text.empty() && whitespace(text.back()))
            text.remove_suffix(1);
        return text;
    }

    bool equal_ascii(std::string_view left, std::string_view right)
    {
        return left.size() == right.size()
            && std::equal(left.begin(), left.end(), right.begin(),
                [](char a, char b) { return (a >= 'A' && a <= 'Z' ? a + ('a' - 'A') : a) == b; });
    }
}

Result inspect(URL::URL const& url, bool decode_source)
{
    using namespace ModulePaths;
    if (url.serialize().bytes_as_string_view().length() > url_limit)
        return { .failure = Failure::Limit, .format = Format::JavaScript, .source = {} };
    // The query is part of the body, while the fragment is only module identity.
    auto serialized = url.serialize(URL::ExcludeFragment::Yes);
    auto bytes = serialized.bytes_as_string_view();
    if (!bytes.starts_with("data:"sv))
        return { .failure = Failure::InvalidURL, .format = Format::JavaScript, .source = {} };
    auto text = std::string_view(bytes.characters_without_null_termination(), bytes.length());
    auto comma = text.find(',');
    if (comma == std::string_view::npos)
        return { .failure = Failure::InvalidURL, .format = Format::JavaScript, .source = {} };
    auto metadata = text.substr(5, comma - 5);
    auto separator = metadata.find(';');
    auto essence = metadata.substr(0, separator);
    auto mime = trim(metadata);
    Result result;
    // Node v24.15 get_format.js deliberately normalizes JavaScript MIME names
    // with a case/whitespace regex, but matches JSON and Wasm essences exactly.
    // Do not broaden those formats by normalizing all MIME names together.
    if (equal_ascii(trim(essence), "text/javascript")
        || equal_ascii(trim(essence), "application/javascript"))
        result.format = Format::JavaScript;
    else if (essence == "application/json")
        result.format = Format::Json;
    else if (essence == "application/wasm")
        return { .failure = Failure::UnsupportedWasm, .format = Format::JavaScript, .source = {} };
    else
        return { .failure = Failure::UnknownFormat, .format = Format::JavaScript, .source = {} };
    if (!decode_source)
        return result;

    bool base64 = false;
    if (mime.size() >= 7 && equal_ascii(mime.substr(mime.size() - 6), "base64")) {
        auto prefix = mime.substr(0, mime.size() - 6);
        while (!prefix.empty() && prefix.back() == ' ')
            prefix.remove_suffix(1);
        base64 = !prefix.empty() && prefix.back() == ';';
    }

    auto body
        = URL::percent_decode(StringView { text.data() + comma + 1, text.size() - comma - 1 });
    if (!base64) {
        if (body.length() > source_limit)
            return { .failure = Failure::Limit, .format = Format::JavaScript, .source = {} };
        result.source.assign(body.characters(), body.length());
        return result;
    }

    // WHATWG forgiving base64 accepts missing padding and ASCII whitespace, but
    // rejects URL alphabet/junk. Remove only that whitespace before AK decoding.
    std::string encoded;
    encoded.reserve(body.length());
    for (char ch : body) {
        if (!whitespace(ch))
            encoded.push_back(ch);
    }

    auto significant = encoded.size();
    if (significant % 4 == 0) {
        if (significant && encoded[significant - 1] == '=')
            --significant;
        if (significant && encoded[significant - 1] == '=')
            --significant;
    }

    if (significant % 4 == 1)
        return { .failure = Failure::InvalidURL, .format = Format::JavaScript, .source = {} };
    for (size_t index = 0; index < significant; ++index) {
        auto ch = encoded[index];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')
                || ch == '+' || ch == '/'))
            return { .failure = Failure::InvalidURL, .format = Format::JavaScript, .source = {} };
    }

    if (significant / 4 * 3 + significant % 4 * 3 / 4 > source_limit)
        return { .failure = Failure::Limit, .format = Format::JavaScript, .source = {} };
    auto decoded = AK::decode_base64(view(encoded), AK::LastChunkHandling::Loose);
    if (decoded.is_error())
        return { .failure = Failure::InvalidURL, .format = Format::JavaScript, .source = {} };
    auto const& buffer = decoded.value();
    if (buffer.size() > source_limit)
        return { .failure = Failure::Limit, .format = Format::JavaScript, .source = {} };
    if (!buffer.is_empty())
        result.source.assign(reinterpret_cast<char const*>(buffer.data()), buffer.size());
    return result;
}
}
