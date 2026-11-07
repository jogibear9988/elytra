/** Stable private wire tags for native typed views, independent of LibJS enum numbering. */
#pragma once
#include <AK/Array.h>
#include <AK/Optional.h>
#include <LibJS/Runtime/TypedArray.h>

namespace Firefly {
struct BinaryViewType {
    u8 tag;
    JS::TypedArrayBase::Kind kind;
    size_t element_size;
};

inline constexpr Array<BinaryViewType, 12> binary_view_types { {
    { 0, JS::TypedArrayBase::Kind::Uint8Array, 1 },
    { 1, JS::TypedArrayBase::Kind::Int8Array, 1 },
    { 2, JS::TypedArrayBase::Kind::Uint8ClampedArray, 1 },
    { 3, JS::TypedArrayBase::Kind::Uint16Array, 2 },
    { 4, JS::TypedArrayBase::Kind::Int16Array, 2 },
    { 5, JS::TypedArrayBase::Kind::Uint32Array, 4 },
    { 6, JS::TypedArrayBase::Kind::Int32Array, 4 },
    { 7, JS::TypedArrayBase::Kind::Float16Array, 2 },
    { 8, JS::TypedArrayBase::Kind::Float32Array, 4 },
    { 9, JS::TypedArrayBase::Kind::Float64Array, 8 },
    { 10, JS::TypedArrayBase::Kind::BigUint64Array, 8 },
    { 11, JS::TypedArrayBase::Kind::BigInt64Array, 8 },
} };

inline Optional<BinaryViewType> binary_view_type(u8 tag)
{
    for (auto type : binary_view_types) {
        if (type.tag == tag)
            return type;
    }

    return {};
}

inline Optional<BinaryViewType> binary_view_type(JS::TypedArrayBase::Kind kind)
{
    for (auto type : binary_view_types) {
        if (type.kind == kind)
            return type;
    }

    return {};
}
}
