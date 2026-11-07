/** Reuse the existing borrowed-origin ABI so every Rust copy consults one native policy. */
#include "OriginPolicy.h"
#include <LibURL/Parser.h>
#include <LibURL/RustIntegration.h>

extern "C" void firefly_url_custom_origin(
    u8 const*, size_t, void*, void (*)(void*, URL::FFI::FfiOrigin const*));

extern "C" void firefly_url_custom_origin(u8 const* data, size_t length, void* context,
    void (*callback)(void*, URL::FFI::FfiOrigin const*))
{
    StringView serialized { reinterpret_cast<char const*>(data), length };
    auto separator = serialized.find(':');
    if (!separator.has_value()
        || !Firefly::Protocols::is_standard_scheme(serialized.substring_view(0, *separator)))
        return;
    // Parsing/host normalization never call origin(), avoiding a Rust/C++ cycle.
    auto url = URL::Parser::basic_parse(serialized);
    if (!url.has_value())
        return;
    auto origin = Firefly::Protocols::custom_origin(*url);
    if (!origin.has_value())
        return;
    URL::RustIntegration::lend_origin_to_ffi(
        *origin, [&](auto const& borrowed) { callback(context, &borrowed); });
}
