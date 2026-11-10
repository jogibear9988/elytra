/** Validate byte snapshots and readiness before reaching the OS-backed encryption provider. */
#include "Host.h"
#include "../../../mite/runtime/platform/WindowsSockets.h"
#include "../platform/GCIncludes.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cstring>
#include <openssl/crypto.h>

namespace Firefly::SafeStorage {
namespace {
    JS::Completion fail(JS::VM& vm, char const* code, char const* message)
    {
        auto error = JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::Completion failure(JS::VM& vm, Status status)
    {
        if (status == Status::Unavailable || status == Status::Keychain)
            return fail(
                vm, "ERR_FIREFLY_SAFE_STORAGE_UNAVAILABLE", "OS key storage is unavailable");
        if (status == Status::Limit)
            return fail(vm, "ERR_OUT_OF_RANGE", "Safe storage byte budget exceeded");
        if (status == Status::InvalidData)
            return fail(vm, "ERR_FIREFLY_SAFE_STORAGE_INVALID_DATA",
                "Invalid, foreign or unauthenticated Firefly ciphertext");
        return fail(
            vm, "ERR_FIREFLY_SAFE_STORAGE_CRYPTO", "Native authenticated encryption failed");
    }

    struct State {
        // Native readiness/provider functions never own JS values.
        using HoldsNoCellEdges = void;
        std::function<bool()> ready;
        Factory factory;
        std::unique_ptr<Backend> backend;

        Backend* get()
        {
            if (!ready())
                return nullptr;
            if (!backend)
                backend = factory();
            return backend.get();
        }
    };
}

void install_host(JS::Realm& realm, JS::Object& host, std::function<bool()> ready, Factory factory)
{
    auto state = std::make_shared<State>(move(ready), move(factory), nullptr);
    host.define_native_function(realm, "safeStorageAvailable"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state)),
            [](auto& state_capture, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& state = state_capture.value;
                auto* backend = state->get();
                return JS::Value(backend && backend->available());
            }),
        0, JS::default_attributes);
    for (auto encrypt : { true, false }) {
        host.define_native_function(realm,
            encrypt ? "safeStorageEncrypt"_utf16 : "safeStorageDecrypt"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(state), encrypt),
                [](auto& state_capture, auto& encrypt,
                    JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& state = state_capture.value;
                    auto* backend = state->get();
                    if (!backend)
                        return fail(
                            vm, "ERR_FIREFLY_NOT_READY", "Safe storage requires app readiness");
                    auto value = vm.argument(0);
                    if (!value.is_object() || !is<JS::Uint8Array>(value.as_object()))
                        return fail(
                            vm, "ERR_INVALID_ARG_TYPE", "Native safe storage requires bytes");
                    auto& array = static_cast<JS::Uint8Array&>(value.as_object());
                    auto witness = JS::make_typed_array_with_buffer_witness_record(
                        array, JS::ArrayBuffer::Order::SeqCst);
                    if (JS::is_typed_array_out_of_bounds(witness))
                        return fail(vm, "ERR_INVALID_ARG_TYPE", "Detached safe storage input");
                    auto length = JS::typed_array_byte_length(witness);
                    if (length > Backend::plaintext_limit + (encrypt ? 0 : 64))
                        return failure(vm, Status::Limit);
                    auto* data = static_cast<uint8_t const*>(
                        array.viewed_array_buffer()->data_at(array.byte_offset()));
                    auto result = encrypt ? backend->encrypt({ data, length })
                                          : backend->decrypt({ data, length });
                    if (result.status != Status::Ok)
                        return failure(vm, result.status);
                    ScopeGuard clear_plaintext([&] {
                        if (!encrypt && !result.bytes.empty())
                            OPENSSL_cleanse(result.bytes.data(), result.bytes.size());
                    });
                    GC::Root output { TRY(
                        JS::Uint8Array::create(vm.realm(), result.bytes.size())) };
                    if (!result.bytes.empty())
                        output->viewed_array_buffer()->overwrite(
                            0, result.bytes.data(), result.bytes.size());
                    return output.ptr();
                }),
            1, JS::default_attributes);
    }
}
}
