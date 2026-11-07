/** Convert native UTF-8 event/error text in the VM owner's realm. */
#pragma once
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly::BridgeDetail {
inline JS::Value text(JS::VM& vm, StringView value)
{
    return JS::PrimitiveString::create(
        vm, Utf16String::from_utf8_with_replacement_character(value));
}
}
