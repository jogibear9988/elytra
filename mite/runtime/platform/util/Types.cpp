/** Inspect LibJS internal classes and backing slots, including revoked Proxies and detached views.
 */
#include "platform/util/Types.h"
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Mite {
GC::Ref<JS::Object> create_util_types_host(JS::Realm& realm)
{
    GC::Root result { JS::Object::create(realm, nullptr) };
    result->define_native_function(
        realm, "isProxy"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(value.is_object() && value.as_object().is_proxy());
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isPromise"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(
                value.is_object() && JS::Promise::is_engine_class_of(value.as_object()));
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isTypedArray"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(
                value.is_object() && JS::TypedArrayBase::is_engine_class_of(value.as_object()));
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isUint8Array"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(value.is_object()
                && JS::TypedArrayBase::is_engine_class_of(value.as_object())
                && static_cast<JS::TypedArrayBase&>(value.as_object()).kind()
                    == JS::TypedArrayBase::Kind::Uint8Array);
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isDataView"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(
                value.is_object() && JS::DataView::is_engine_class_of(value.as_object()));
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isArrayBufferView"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(value.is_object()
                && (JS::DataView::is_engine_class_of(value.as_object())
                    || JS::TypedArrayBase::is_engine_class_of(value.as_object())));
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isAnyArrayBuffer"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(
                value.is_object() && JS::ArrayBuffer::is_engine_class_of(value.as_object()));
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isArrayBuffer"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(value.is_object()
                && JS::ArrayBuffer::is_engine_class_of(value.as_object())
                && !static_cast<JS::ArrayBuffer&>(value.as_object()).is_shared_array_buffer());
        },
        1, JS::default_attributes);
    result->define_native_function(
        realm, "isSharedArrayBuffer"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            return JS::Value(value.is_object()
                && JS::ArrayBuffer::is_engine_class_of(value.as_object())
                && static_cast<JS::ArrayBuffer&>(value.as_object()).is_shared_array_buffer());
        },
        1, JS::default_attributes);
    return *result;
}
}
