/** Internal binary snapshots: exact raw bytes, explicit policy and independent realm ownership. */
#include "BridgeDataTest.h"
#include "serialization/BridgeData.h"
#include <AK/ByteBuffer.h>
#include <AK/StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>

ErrorOr<void> check_binary_wire(JS::VM& vm, JS::Realm& source, JS::Realm& target)
{
    auto run = [&](Utf16View text, JS::Realm& realm) -> ErrorOr<JS::Value> {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), realm);
        if (parsed.is_error())
            return Error::from_string_literal("Invalid binary fixture");
        GC::Root script { parsed.release_value() };
        auto value = vm.run(*script);
        if (value.is_error())
            return Error::from_string_literal("Binary fixture threw");
        return value.release_value();
    };

    auto check = [&](Utf16View text, JS::Realm& realm) -> ErrorOr<void> {
        auto value = TRY(run(text, realm));
        if (!value.is_boolean() || !value.as_bool())
            return Error::from_string_literal("Binary snapshot assertion failed");
        return {};
    };

    Firefly::BridgeData::Limits policy;
    policy.binary_bytes = 65536;
    auto value = TRY(run(uR"JS(
        (()=>{globalThis.binaryHooks=0;const buffer=new ArrayBuffer(65536),view=new Uint8Array(buffer);
        for(let i=0;i<view.length;i++)view[i]=(i*37+11)&255;
        Object.defineProperty(buffer,'trap',{get(){binaryHooks++;throw Error('getter');}});
        Object.setPrototypeOf(buffer,new Proxy({},{get(){binaryHooks++;throw Error('prototype');}}));
        const root={buffer,alias:buffer,empty:new ArrayBuffer(0)};root.self=root;
        globalThis.binaryOriginal=root;return root})()
    )JS"sv,
        source));
    if (!Firefly::BridgeData::capture(value, source, {}).is_error())
        return Error::from_string_literal("Default publication admitted binary data");
    auto data = TRY(Firefly::BridgeData::capture(value, source, policy));
    auto wire = TRY(data.encode_wire(policy));
    if (!wire.starts_with("FFB2"sv) || wire.length() > 65536 + 1024
        || !data.encode_wire({}).is_error()
        || !Firefly::BridgeData::decode_wire(wire, {}).is_error())
        return Error::from_string_literal("Binary framing/policy/size assertion failed");
    auto smaller = policy;
    --smaller.binary_bytes;
    if (!data.encode_wire(smaller).is_error()
        || !Firefly::BridgeData::decode_wire(wire, smaller).is_error())
        return Error::from_string_literal("Binary exact byte budget not enforced");
    TRY(check(u"new Uint8Array(binaryOriginal.buffer).fill(0);binaryHooks===0"sv, source));
    vm.heap().collect_garbage();
    auto decoded = TRY(Firefly::BridgeData::decode_wire(wire, policy));
    if (!decoded.materialize(target).is_error())
        return Error::from_string_literal("Frozen publication admitted mutable binary storage");
    auto copied = decoded.materialize(target, {}, Firefly::BridgeData::Materialization::Mutable);
    if (copied.is_error())
        return Error::from_string_literal("Cannot materialize binary data");
    target.global_object().define_direct_property(
        "binaryCopy"_utf16, copied.value(), JS::default_attributes);
    vm.heap().collect_garbage();
    TRY(check(uR"JS(
        binaryCopy.buffer instanceof ArrayBuffer && binaryCopy.empty instanceof ArrayBuffer &&
        binaryCopy.buffer===binaryCopy.alias && binaryCopy.self===binaryCopy && binaryCopy.empty.byteLength===0 &&
        !Object.hasOwn(binaryCopy.buffer,'trap') &&
        new Uint8Array(binaryCopy.buffer).every((x,i)=>x===((i*37+11)&255))
    )JS"sv,
        target));
    TRY(check(u"new Uint8Array(binaryCopy.buffer).fill(99);true"sv, target));
    TRY(check(u"new Uint8Array(binaryOriginal.buffer).every(x=>x===0)&&binaryHooks===0"sv, source));
    if (TRY(decoded.encode_wire(policy)) != wire)
        return Error::from_string_literal("Destination mutation changed native binary snapshot");
    for (auto code :
        { u"new ArrayBuffer(1,{maxByteLength:2})"sv,
            u"(()=>{const b=new ArrayBuffer(1);b.transfer();return b})()"sv, u"new Uint8Array(3)"sv,
            u"new Proxy(new ArrayBuffer(1),{get(){throw Error('trap')}})"sv }) {
        auto bad = TRY(run(code, source));
        if (!Firefly::BridgeData::capture(bad, source, policy).is_error())
            return Error::from_string_literal("Unsupported binary source admitted");
    }

    for (size_t size : { 0uz, 4uz, 7uz, wire.length() - 1 }) {
        if (!Firefly::BridgeData::decode_wire(wire.substring_view(0, size), policy).is_error())
            return Error::from_string_literal("Truncated binary frame admitted");
    }

    auto oversized_header = TRY(ByteBuffer::copy(wire.bytes()));
    for (size_t i = 4; i < 8; ++i)
        oversized_header[i] = 255;
    if (!Firefly::BridgeData::decode_wire(
            StringView(
                reinterpret_cast<char const*>(oversized_header.data()), oversized_header.size()),
            policy)
            .is_error())
        return Error::from_string_literal("Oversized binary metadata admitted");
    // Reject unauthenticated extras, even when the configured payload budget has room.
    auto larger = policy;
    ++larger.binary_bytes;
    auto trailing = ByteString::formatted("{}x", wire);
    if (!Firefly::BridgeData::decode_wire(trailing, larger).is_error())
        return Error::from_string_literal("Unclaimed binary trailing bytes admitted");
    auto remote = [](StringView metadata) {
        StringBuilder frame;
        frame.append("FFB2"sv);
        for (size_t i = 0; i < 4; ++i)
            frame.append(static_cast<char>((metadata.length() >> (i * 8)) & 255));
        frame.append(metadata);
        return frame.to_byte_string();
    };

    auto empty = TRY(Firefly::BridgeData::decode_wire(remote("[2,[[8,0]]]"sv), policy));
    if (!empty.encode_wire({}).is_error())
        return Error::from_string_literal("Empty buffer bypassed default binary policy");
    for (auto metadata :
        { "[2,[[8,-1]]]"sv, "[2,[[8,0.5]]]"sv, "[2,[[8,1]]]"sv, "[2,[[8,0,[]]]]"sv, "[2,[[7,0]]]"sv,
            "[2,[[9,0]]]"sv, "[2,[[8,0],[8,0]]]"sv, "[2,[[0]]]"sv, "[1,[[8,0]]]"sv }) {
        if (!Firefly::BridgeData::decode_wire(remote(metadata), policy).is_error())
            return Error::from_string_literal(
                "Malformed binary schema/length/reachability admitted");
    }

    outln("PASS internal raw binary snapshots, exact budgets, aliases, GC, mutable ownership and "
          "default/frozen policy refusal");
    return {};
}
