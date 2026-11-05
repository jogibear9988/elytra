#include "Crypto.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace Mite {
namespace {
JS::Completion hash_error(JS::VM& vm,char const* code,Utf16View message)
{
    auto error=JS::Error::create(vm.realm(),message);
    if(code)error->define_direct_property("code"_utf16,JS::PrimitiveString::create(vm,Utf16String::from_utf8(StringView(code,strlen(code)))),JS::default_attributes);
    return JS::throw_completion(error);
}
struct HashState {
    EVP_MD_CTX* context {nullptr};
    EVP_MD const* method {nullptr};
    size_t output_length {0};
    bool xof {false};
    bool finalized {false};
    // Node streams may finalize before a manual digest call. Cache bytes, not
    // a live provider context, for that path; the public JS state rejects repeats.
    std::vector<unsigned char> digest;
    ~HashState(){close();}
    void close(){EVP_MD_CTX_free(context);context=nullptr;}
};
JS::ThrowCompletionOr<std::optional<size_t>> output_length(JS::VM& vm,size_t argument)
{
    auto value=vm.argument(argument);
    if(value.is_undefined())return std::optional<size_t>{};
    if(!value.is_number())return vm.throw_completion<JS::TypeError>("Invalid internal hash output length"sv);
    auto number=value.as_double();
    if(!std::isfinite(number) || number<0 || number>2147483647 || std::trunc(number)!=number)
        return vm.throw_completion<JS::RangeError>("Invalid internal hash output length"sv);
    return std::optional<size_t>{static_cast<size_t>(number)};
}
JS::ThrowCompletionOr<void> configure_length(JS::VM& vm,HashState& state,std::optional<size_t> requested)
{
    auto size=EVP_MD_get_size(state.method);
    if(size<0)return hash_error(vm,"ERR_CRYPTO_HASH_CREATION_FAILED",u"Hash digest size is unavailable"sv);
    state.xof=(EVP_MD_get_flags(state.method)&EVP_MD_FLAG_XOF)!=0;
    if(requested && !state.xof && *requested!=static_cast<size_t>(size))
        return hash_error(vm,"ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH",u"Invalid hash output length"sv);
    state.output_length=requested.value_or(static_cast<size_t>(size));
    return {};
}
GC::Ref<JS::Object> hash_handle(JS::Realm& realm,std::shared_ptr<HashState> state)
{
    GC::Root result {JS::Object::create(realm,nullptr)};
    result->define_native_function(realm,"update"_utf16,[state](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        if(!state->context)return JS::Value(false);
        auto value=vm.argument(0);
        if(!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))return vm.throw_completion<JS::TypeError>("Invalid internal hash bytes"sv);
        auto& array=static_cast<JS::Uint8Array&>(value.as_object());
        auto witness=JS::make_typed_array_with_buffer_witness_record(array,JS::ArrayBuffer::Order::SeqCst);
        if(JS::is_typed_array_out_of_bounds(witness))return JS::Value(EVP_DigestUpdate(state->context,nullptr,0)==1);
        auto size=JS::typed_array_byte_length(witness);
        ERR_clear_error();
        int status=size?array.viewed_array_buffer()->with_readonly_bytes(array.byte_offset(),size,[&](ReadonlyBytes bytes){return EVP_DigestUpdate(state->context,bytes.data(),bytes.size());}):EVP_DigestUpdate(state->context,nullptr,0);
        return JS::Value(status==1);
    },1,JS::default_attributes);
    result->define_native_function(realm,"digest"_utf16,[state,&realm](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        if(!state->finalized){
            if(!state->context)return hash_error(vm,"ERR_CRYPTO_HASH_FINALIZED",u"Digest already called"sv);
            std::vector<unsigned char> bytes(state->output_length);
            auto* data=bytes.empty()?nullptr:bytes.data();
            unsigned size=0;
            ERR_clear_error();
            int status=state->xof?EVP_DigestFinalXOF(state->context,data,bytes.size()):EVP_DigestFinal_ex(state->context,data,&size);
            state->close();
            if(status!=1 || (!state->xof && size!=bytes.size()))return hash_error(vm,"ERR_CRYPTO_HASH_FINALIZED",u"Hash finalization failed"sv);
            state->digest=std::move(bytes);state->finalized=true;
        }
        GC::Root bytes {TRY(JS::Uint8Array::create(realm,state->digest.size()))};
        if(!state->digest.empty())bytes->viewed_array_buffer()->overwrite(0,state->digest.data(),state->digest.size());
        return bytes.ptr();
    },0,JS::default_attributes);
    result->define_native_function(realm,"copy"_utf16,[state,&realm](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        if(!state->context)return hash_error(vm,"ERR_CRYPTO_HASH_FINALIZED",u"Digest already called"sv);
        auto requested=TRY(output_length(vm,0));
        auto copy=std::make_shared<HashState>();copy->method=state->method;
        TRY(configure_length(vm,*copy,requested.value_or(state->output_length)));
        copy->context=EVP_MD_CTX_new();
        ERR_clear_error();
        if(!copy->context || EVP_MD_CTX_copy_ex(copy->context,state->context)!=1)return hash_error(vm,"ERR_CRYPTO_HASH_CREATION_FAILED",u"Hash copy failed"sv);
        return hash_handle(realm,std::move(copy));
    },1,JS::default_attributes);
    return *result;
}
}
GC::Ref<JS::Object> create_hash_host(JS::Realm& realm)
{
    GC::Root host {JS::Object::create(realm,nullptr)};
    host->define_native_function(realm,"create"_utf16,[&realm](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        if(!vm.argument(0).is_string())return vm.throw_completion<JS::TypeError>("Invalid internal hash algorithm"sv);
        auto name=MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        auto requested=TRY(output_length(vm,1));
        ERR_clear_error();
        // get_digestbyname returns a borrowed static descriptor. Provider init
        // may still fail (for example legacy MD4); never assert on that failure.
        auto* method=EVP_get_digestbyname(name.characters());
        if(!method)return hash_error(vm,nullptr,u"Digest method not supported"sv);
        auto state=std::make_shared<HashState>();state->method=method;
        TRY(configure_length(vm,*state,requested));
        state->context=EVP_MD_CTX_new();
        if(!state->context || EVP_DigestInit_ex(state->context,method,nullptr)!=1)
            return hash_error(vm,"ERR_OSSL_EVP_UNSUPPORTED",u"Hash provider is unavailable"sv);
        return hash_handle(realm,std::move(state));
    },2,JS::default_attributes);
    return *host;
}
}
