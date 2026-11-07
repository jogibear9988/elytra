/** Native view graphs preserve byte geometry and backing identity without invoking JS hooks. */
#include "BridgeDataTest.h"
#include "serialization/BridgeData.h"
#include <AK/StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>

ErrorOr<void> check_binary_views(JS::VM& vm, JS::Realm& source, JS::Realm& target)
{
    auto run = [&](Utf16View code, JS::Realm& realm) -> ErrorOr<JS::Value> {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(code), realm);
        if (parsed.is_error())
            return Error::from_string_literal("Invalid binary view test");
        GC::Root script { parsed.release_value() };
        auto value = vm.run(*script);
        if (value.is_error())
            return Error::from_string_literal("Binary view fixture threw");
        return value.release_value();
    };

    auto check = [&](Utf16View code, JS::Realm& realm) -> ErrorOr<void> {
        auto value = TRY(run(code, realm));
        if (!value.is_boolean() || !value.as_bool())
            return Error::from_string_literal("Binary view assertion failed");
        return {};
    };

    Firefly::BridgeData::Limits policy;
    policy.binary_bytes = 65536;
    auto value = TRY(run(uR"JS((()=>{
        globalThis.viewHooks=0;
        const buffer=new ArrayBuffer(65536),bytes=new Uint8Array(buffer);
        for(let i=0;i<bytes.length;i++)bytes[i]=(i*37+11)&255;
        class BufferLike extends Uint8Array {}
        const view=new BufferLike(buffer,5,17);
        for(const name of ['buffer','byteLength','byteOffset','length','constructor'])
            Object.defineProperty(view,name,{get(){viewHooks++;throw Error('getter')}});
        Object.setPrototypeOf(view,new Proxy({},{get(){viewHooks++;throw Error('prototype')}}));
        const constructors=[Uint8Array,Int8Array,Uint8ClampedArray,Uint16Array,Int16Array,
            Uint32Array,Int32Array,Float16Array,Float32Array,Float64Array,BigUint64Array,BigInt64Array];
        const root={view,alias:view,data:new DataView(buffer,3,19),buffer,
            views:constructors.map(C=>new C(buffer,8,4)),tail:new Uint8Array(buffer,65536,0)};
        root.self=root;globalThis.viewOriginal=root;return root;
    })())JS"sv,
        source));
    if (!Firefly::BridgeData::capture(value, source, {}).is_error())
        return Error::from_string_literal("Default policy admitted a binary view");
    auto snapshot = TRY(Firefly::BridgeData::capture(value, source, policy));
    auto bytes = TRY(snapshot.encode_wire(policy));
    if (bytes.length() > 65536 + 4096)
        return Error::from_string_literal("Multiple views duplicated backing payload");
    auto decoded = TRY(Firefly::BridgeData::decode_wire(bytes, policy));
    if (!decoded.materialize(target).is_error())
        return Error::from_string_literal("Frozen publication admitted binary views");
    auto copied = decoded.materialize(target, {}, Firefly::BridgeData::Materialization::Mutable);
    if (copied.is_error())
        return Error::from_string_literal("Cannot materialize binary views");
    target.global_object().define_direct_property(
        "viewCopy"_utf16, copied.value(), JS::default_attributes);
    vm.heap().collect_garbage();
    TRY(check(uR"JS((()=>{
        const c=viewCopy,constructors=[Uint8Array,Int8Array,Uint8ClampedArray,Uint16Array,Int16Array,
            Uint32Array,Int32Array,Float16Array,Float32Array,Float64Array,BigUint64Array,BigInt64Array];
        return c.self===c&&c.view===c.alias&&c.view instanceof Uint8Array&&c.view.buffer===c.buffer&&
            c.view.byteOffset===5&&c.view.length===17&&!Object.hasOwn(c.view,'constructor')&&
            c.view.every((x,i)=>x===(((i+5)*37+11)&255))&&
            c.data instanceof DataView&&c.data.buffer===c.buffer&&c.data.byteOffset===3&&c.data.byteLength===19&&
            c.data.getUint8(0)===((3*37+11)&255)&&c.tail.buffer===c.buffer&&c.tail.byteOffset===65536&&c.tail.length===0&&
            c.views.every((v,i)=>v instanceof constructors[i]&&v.buffer===c.buffer&&v.byteOffset===8&&v.length===4);
    })())JS"sv,
        target));
    TRY(check(
        u"viewHooks===0&&new Uint8Array(viewOriginal.buffer)[5]===((5*37+11)&255)"sv, source));
    TRY(check(
        u"viewCopy.data.setUint8(2,77);viewCopy.view[0]===77&&viewCopy.alias[0]===77"sv, target));
    TRY(check(
        u"viewHooks===0&&new Uint8Array(viewOriginal.buffer)[5]===((5*37+11)&255)"sv, source));
    if (TRY(decoded.encode_wire(policy)) != bytes)
        return Error::from_string_literal("View mutation escaped destination storage");
    auto small = policy;
    small.binary_bytes = 17;
    auto selected = TRY(run(u"new Uint8Array(viewOriginal.buffer,5,17)"sv, source));
    if (!Firefly::BridgeData::capture(selected, source, small).is_error())
        return Error::from_string_literal("View bypassed full backing byte budget");
    if (!Firefly::BridgeData::capture(copied.value(), source, policy).is_error())
        return Error::from_string_literal("Foreign realm view graph admitted");
    auto shared = JS::ArrayBuffer::create(source, 8, JS::DataBlock::Shared::Yes);
    if (shared.is_error())
        return Error::from_string_literal("Cannot construct native shared-buffer negative control");
    source.global_object().define_direct_property(
        "sharedViewBacking"_utf16, shared.release_value().ptr(), JS::default_attributes);
    for (auto code : { u"new Uint8Array(new ArrayBuffer(8,{maxByteLength:16}))"sv,
             u"new Uint8Array(sharedViewBacking)"sv, u"new DataView(sharedViewBacking)"sv,
             u"new DataView(new ArrayBuffer(8,{maxByteLength:16}),0,4)"sv,
             u"(()=>{const b=new ArrayBuffer(8),v=new Uint8Array(b);b.transfer();return v})()"sv,
             u"(()=>{const b=new ArrayBuffer(8),v=new DataView(b);b.transfer();return v})()"sv,
             u"new Proxy(new Uint8Array(4),{get(){throw Error('trap')}})"sv }) {
        auto bad = TRY(run(code, source));
        if (!Firefly::BridgeData::capture(bad, source, policy).is_error())
            return Error::from_string_literal("Unsupported binary view backing admitted");
    }

    auto remote = [](StringView metadata) {
        StringBuilder frame;
        frame.append("FFB2"sv);
        for (size_t i = 0; i < 4; ++i)
            frame.append(static_cast<char>((metadata.length() >> (i * 8)) & 255));
        frame.append(metadata);
        frame.append("12345678"sv);
        return frame.to_byte_string();
    };

    for (auto metadata : {
             "[2,[[9,255,1,0,1],[8,8]]]"sv, // Unknown view kind.
             "[2,[[9,0,0,0,1],[8,8]]]"sv, // Self instead of backing storage.
             "[2,[[9,0,2,0,1],[8,8]]]"sv, // Dangling backing reference.
             "[2,[[9,3,1,1,1],[8,8]]]"sv, // Unaligned Uint16 offset.
             "[2,[[9,9,1,0,2],[8,8]]]"sv, // Float64 length overflows backing.
             "[2,[[9,0,1,9,0],[8,8]]]"sv, // Zero-length view past end.
             "[2,[[9,0,1,0.5,1],[8,8]]]"sv,
             "[2,[[10,1,7,2],[8,8]]]"sv,
             "[2,[[10,1,0,-1],[8,8]]]"sv,
             "[2,[[10,1,0,1],[8,8],[8,0]]]"sv, // Unreachable storage.
         }) {
        if (!Firefly::BridgeData::decode_wire(remote(metadata), policy).is_error())
            return Error::from_string_literal("Malformed binary view geometry/reference admitted");
    }

    outln("PASS internal native typed/DataView snapshots, backing aliases/offsets, trap-free "
          "subclass normalization and malformed geometry refusal");
    return {};
}
