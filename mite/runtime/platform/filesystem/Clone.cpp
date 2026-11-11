/** Query the actual source volume; copies themselves remain libuv file operations. */
#include "platform/filesystem/Clone.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#ifdef __APPLE__
#include <sys/attr.h>
#endif
namespace Mite {
GC::Ref<JS::Object> create_file_clone_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(
        realm, "isCloneSupported"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return vm.throw_completion<JS::TypeError>("path must be a string"_utf16);
            auto converted = vm.argument(0).as_string().utf16_string_view().to_byte_string();
            if (converted.is_error())
                return vm.throw_completion<JS::TypeError>(
                    "Invalid clone query path encoding"_utf16);
            auto encoded = converted.release_value();
            if (encoded.length() > 32768 || encoded.contains('\0'))
                return vm.throw_completion<JS::TypeError>("Invalid clone query path"_utf16);
#ifdef __APPLE__
            struct attrlist attributes {};
            attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
            attributes.volattr = ATTR_VOL_CAPABILITIES;
            struct {
                uint32_t length;
                vol_capabilities_attr_t capabilities;
            } result {};
            if (getattrlist(encoded.characters(), &attributes, &result, sizeof(result), 0))
                return JS::Value(false);
            return JS::Value(
                (result.capabilities.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_CLONE)
                && (result.capabilities.capabilities[VOL_CAPABILITIES_INTERFACES]
                    & VOL_CAP_INT_CLONE));
#else
            auto error = JS::Error::create(
                vm.realm(), "Native volume clone queries are not verified on this platform"_utf16);
            error->define_direct_property("code"_utf16,
                JS::PrimitiveString::create(vm, "ERR_MITE_UNSUPPORTED_API"_utf16),
                JS::default_attributes);
            return JS::throw_completion(error);
#endif
        },
        1, JS::default_attributes);
    return host;
}
}
