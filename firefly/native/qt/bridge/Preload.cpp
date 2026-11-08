/** Validate preload bytes/paths and configure the owned native preload session. */
#include "Impl.h"
#include "Value.h"
#include "modules/ModulePaths.h"
#include <AK/JsonObject.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/VM.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <UI/Qt/WebContentView.h>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace Firefly {
using BridgeDetail::text;

JS::Completion Bridge::Impl::preload_error(JS::VM& vm, char const* code, StringView message)
{
    auto error
        = JS::Error::create(realm, Utf16String::from_utf8_with_replacement_character(message));
    error->define_direct_property(
        "code"_utf16, text(vm, StringView(code, strlen(code))), JS::default_attributes);
    return JS::throw_completion(error);
}

JS::ThrowCompletionOr<Optional<Bridge::Impl::Preload>> Bridge::Impl::read_preload(
    JS::VM& vm, size_t preload_argument, size_t arguments_argument)
{
    auto arguments = TRY(read_process_arguments(vm, arguments_argument));
    auto value = vm.argument(preload_argument);
    if (value.is_undefined()) {
        if (!arguments.is_empty())
            return preload_error(vm, "ERR_FIREFLY_UNSUPPORTED_API",
                "additionalArguments requires a configured preload"sv);
        return Optional<Preload> {};
    }

    if (!value.is_string())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "preload must be a string"sv);
    auto encoded = value.as_string().utf16_string_view().to_byte_string();
    if (encoded.is_error())
        return preload_error(
            vm, "ERR_INVALID_ARG_VALUE", "Preload filename must encode as UTF-8"sv);
    auto bytes = encoded.release_value();
    if (bytes.contains('\0') || bytes.length() > 4096)
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid preload filename"sv);
    auto path = Mite::ModulePaths::path_from_utf8(std::string(bytes.characters(), bytes.length()));
    if (!path.is_absolute())
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "preload must be absolute"sv);
    if (path.extension() == ".mjs")
        return preload_error(
            vm, "ERR_FIREFLY_UNSUPPORTED_API", "ES module preloads are not implemented"sv);
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
        return preload_error(
            vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Preload is not a readable regular file"sv);
    std::ifstream input(path, std::ios::binary);
    std::string contents(65537, '\0');
    input.read(contents.data(), contents.size());
    auto count = static_cast<size_t>(input.gcount());
    if (input.bad() || (!input.eof() && input.fail()))
        return preload_error(vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Cannot read preload"sv);
    if (count > 65536)
        return preload_error(vm, "ERR_FIREFLY_PRELOAD_TOO_LARGE", "Preload exceeds 64 KiB"sv);
    contents.resize(count);
    auto source = String::from_utf8(StringView(contents.data(), contents.size()));
    if (source.is_error())
        return preload_error(
            vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Preload must be valid UTF-8"sv);
    auto process = TRY(read_process_config(vm, move(arguments)));
    size_t process_total = process.bytes;
    size_t total = count, configured = 1;
    for (auto const& [id, state] : windows)
        if (state.preload.has_value()) {
            total += state.preload->source.bytes().size();
            process_total += state.preload->process.bytes;
            ++configured;
        }

    if (total > 1024 * 1024 || process_total > 1024 * 1024 || configured > 16)
        return preload_error(
            vm, "ERR_FIREFLY_PRELOAD_TOO_LARGE", "Preload window/source budget exceeded"sv);
    auto filename = Mite::ModulePaths::path_string(path);
    auto dirname = Mite::ModulePaths::path_string(path.parent_path());
    return Optional<Preload> { Preload { source.release_value(),
        MUST(String::from_utf8(StringView(filename.data(), filename.size()))),
        MUST(String::from_utf8(StringView(dirname.data(), dirname.size()))), move(process) } };
}

void Bridge::Impl::configure_preloads(WebView::WebContentPage& page)
{
    if (stopping)
        return;
    Web::WebDriver::BiDiSessionState state;
    state.is_active = true;
    for (auto const& [id, window] : windows) {
        if (window.closing || !window.preload.has_value())
            continue;
        auto const& preload = *window.preload;
        JsonObject metadata;
        metadata.set("filename"sv, preload.filename);
        metadata.set("dirname"sv, preload.dirname);
        metadata.set("process"sv, PreloadProcess::encode(preload.process));
        if (electron_compat)
            metadata.set("electronCompat"sv, true);
        state.preload_scripts.append({ "firefly.preload.v1"_string, preload.source, move(metadata),
            Vector<String> { window.view->handle() }, {}, {} });
    }

    page.client().async_set_webdriver_bidi_session_state(move(state));
}
}
