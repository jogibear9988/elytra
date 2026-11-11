#include "BridgeDataTest.h"
#include "serialization/BridgeData.h"
#include <AK/ByteBuffer.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>

ErrorOr<void> check_bridge_wire(JS::VM& vm, JS::Realm& source, JS::Realm& target)
{
    auto run = [&](Utf16View code, JS::Realm& realm) -> JS::ThrowCompletionOr<JS::Value> {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(code), realm);
        if (parsed.is_error())
            return vm.throw_completion<JS::SyntaxError>("Invalid wire test fixture"_utf16);
        GC::Root script { parsed.release_value() };
        return vm.run(*script);
    };

    auto check = [&](Utf16View code, JS::Realm& realm) -> ErrorOr<void> {
        auto answer = run(code, realm);
        if (answer.is_error() || !answer.value().is_boolean() || !answer.value().as_bool())
            return Error::from_string_literal("Wire JS assertion failed");
        return {};
    };

    auto value = run(uR"JS(
        (()=>{const child={n:42},a={left:child,right:child,zero:-0,nan:NaN,inf:Infinity,negative:-Infinity,
            text:'\ud800\u0000\udfff',tiny:Number.MIN_VALUE,big:Number.MAX_VALUE,decimal:1.2345678901234567,nil:null,u:undefined,no:false,yes:true};
        a.self=a;a.array=[,child,undefined];a.empty=Object.create(null);
        Object.defineProperty(a,'__proto__',{value:child,enumerable:false});
        Object.defineProperty(a,'\udfff',{value:7,enumerable:true});
        globalThis.wireOriginal=a;return a})()
    )JS"sv,
        source);
    if (value.is_error())
        return Error::from_string_literal("Wire source fixture failed");
    auto captured = TRY(Firefly::BridgeData::capture(value.value(), source, {}));
    auto bytes = TRY(captured.encode_wire({}));
    TRY(check(u"wireOriginal.left.n=99;delete globalThis.wireOriginal;true"sv, source));
    vm.heap().collect_garbage();
    auto decoded = TRY(Firefly::BridgeData::decode_wire(bytes, {}));
    auto copy = decoded.materialize(target);
    if (copy.is_error())
        return Error::from_string_literal("Wire materialization failed");
    target.global_object().define_direct_property(
        "wireCopy"_utf16, copy.value(), JS::default_attributes);
    TRY(check(uR"JS(
        wireCopy.self===wireCopy && wireCopy.left===wireCopy.right && wireCopy.left.n===42 &&
        wireCopy.array.length===3 && !(0 in wireCopy.array) && wireCopy.array[1]===wireCopy.left &&
        (2 in wireCopy.array) && wireCopy.array[2]===undefined && wireCopy.u===undefined && wireCopy.nil===null &&
        wireCopy.tiny===Number.MIN_VALUE && wireCopy.big===Number.MAX_VALUE && wireCopy.decimal===1.2345678901234567 &&
        wireCopy.no===false && wireCopy.yes===true && Object.is(wireCopy.zero,-0) && Number.isNaN(wireCopy.nan) &&
        wireCopy.inf===Infinity && wireCopy.negative===-Infinity && wireCopy.text.length===3 &&
        wireCopy.text.charCodeAt(0)===0xd800 && wireCopy.text.charCodeAt(1)===0 && wireCopy.text.charCodeAt(2)===0xdfff &&
        wireCopy['\udfff']===7 && Object.getPrototypeOf(wireCopy)===Object.prototype &&
        Object.getPrototypeOf(wireCopy.empty)===null && Object.getPrototypeOf(wireCopy.array)===Array.prototype &&
        Object.hasOwn(wireCopy,'__proto__') && wireCopy.__proto__===wireCopy.left && !Object.keys(wireCopy).includes('__proto__') &&
        Object.isFrozen(wireCopy) && Object.isFrozen(wireCopy.array) && Object.isFrozen(wireCopy.left)
    )JS"sv,
        target));
    if (TRY(decoded.encode_wire({})) != bytes)
        return Error::from_string_literal("Wire encoding is not canonical after decode");
    auto mutable_copy
        = decoded.materialize(target, {}, Firefly::BridgeData::Materialization::Mutable);
    if (mutable_copy.is_error())
        return Error::from_string_literal("Mutable wire copy failed");
    target.global_object().define_direct_property(
        "mutableWire"_utf16, mutable_copy.value(), JS::default_attributes);
    TRY(check(
        u"mutableWire.left.n=7;mutableWire.array.push(9);mutableWire.left===mutableWire.right && mutableWire.self===mutableWire && mutableWire.left.n===7 && mutableWire.array.length===4 && wireCopy.left.n===42 && wireCopy.array.length===3 && !Object.isFrozen(mutableWire) && Object.isFrozen(wireCopy)"sv,
        target));
    // Handwritten remote input, independent of our encoder: own __proto__ must
    // remain an inert key, and a self edge must resolve only after allocation.
    auto manual = TRY(Firefly::BridgeData::decode_wire(
        "[1,[[5,true,[[[95,95,112,114,111,116,111,95,95],0,true]]]]]"sv, {}));
    auto manual_copy = manual.materialize(target);
    if (manual_copy.is_error())
        return Error::from_string_literal("Manual wire graph rejected");
    target.global_object().define_direct_property(
        "manualWire"_utf16, manual_copy.value(), JS::default_attributes);
    TRY(check(
        u"Object.getPrototypeOf(manualWire)===null && manualWire.__proto__===manualWire && Object.isFrozen(manualWire)"sv,
        target));
    for (auto bad : {
             ""sv,
             "null"sv,
             "{}"sv,
             "[1,[]]"sv,
             "[2,[[0]]]"sv,
             "[true,[[0]]]"sv,
             "[1.5,[[0]]]"sv,
             "[1,[[0]],0]"sv,
             "[1,[[7,0]]]"sv,
             "[1,[[0,0]]]"sv,
             "[1,[[2,1]]]"sv,
             "[1,[[3,true]]]"sv,
             "[1,[[3,\"oops\"]]]"sv,
             "[1,[[3,1e999]]]"sv,
             "[1,[[3,\"7ff0000000000001\"]]]"sv,
             "[1,[[3,\"fff8000000000000\"]]]"sv,
             "[1,[[3,\"7ff800000000000g\"]]]"sv,
             "[1,[[4,[65536]]]]"sv,
             "[1,[[4,[-1]]]]"sv,
             "[1,[[4,[1.5]]]]"sv,
             "[1,[[5,0,[]]]]"sv,
             "[1,[[6,1.5,[]]]]"sv,
             "[1,[[6,4097,[]]]]"sv,
             "[1,[[5,false,[[[],99,true]]]]]"sv,
             "[1,[[5,false,[[[],0.5,true]]]]]"sv,
             "[1,[[5,false,[[[],0,1]]]]]"sv,
             "[1,[[5,false,[[[],0,true],[[],0,false]]]]]"sv,
             "[1,[[6,0,[[[48],0,true]]]]]"sv,
             "[1,[[6,1,[[[49],0,true]]]]]"sv,
             "[1,[[6,0,[[[108,101,110,103,116,104],0,true]]]]]"sv,
             "[1,[[0],[1]]]"sv,
             "[[[[[[[[[0]]]]]]]]]"sv,
             "[1,[[3,\"unterminated]]]"sv,
         }) {
        if (!Firefly::BridgeData::decode_wire(bad, {}).is_error()) {
            warnln("Accepted malformed wire: {}", bad);
            return Error::from_string_literal("Malformed wire accepted");
        }
    }
    // Bounded malformed byte mutations must reject or normalize to another
    // valid data graph, never abort, loop, execute script or poison later loads.
    for (size_t i = 0; i < bytes.length(); ++i) {
        auto mutation = TRY(ByteBuffer::copy(bytes.bytes()));
        mutation[i] = static_cast<u8>((i * 73) & 127);
        auto candidate = Firefly::BridgeData::decode_wire(
            StringView(reinterpret_cast<char const*>(mutation.data()), mutation.size()), {});
        if (!candidate.is_error())
            (void)TRY(candidate.value().encode_wire({}));
    }

    StringBuilder huge;
    huge.append(bytes);
    for (size_t i = bytes.length(); i < 1024 * 1024; ++i)
        huge.append(' ');
    (void)TRY(Firefly::BridgeData::decode_wire(huge.string_view(), {}));
    huge.append(' ');
    if (!Firefly::BridgeData::decode_wire(huge.string_view(), {}).is_error())
        return Error::from_string_literal("Missing wire byte limit");
    Firefly::BridgeData::Limits limits;
    limits.values = 1;
    if (!Firefly::BridgeData::decode_wire(bytes, limits).is_error()
        || !captured.encode_wire(limits).is_error())
        return Error::from_string_literal("Wire node limit not enforced both ways");
    limits = {};
    limits.depth = 0;
    if (!manual.encode_wire(limits).is_error())
        return Error::from_string_literal("Wire depth limit not enforced");
    limits = {};
    limits.string_code_units = 1;
    if (!Firefly::BridgeData::decode_wire(bytes, limits).is_error())
        return Error::from_string_literal("Wire string limit missing");
    limits = {};
    limits.properties = 0;
    if (!Firefly::BridgeData::decode_wire("[1,[[6,0,[]]]]"sv, limits).is_error())
        return Error::from_string_literal("Array metadata bypassed property budget");
    limits = {};
    limits.values = 4097;
    if (!manual.encode_wire(limits).is_error())
        return Error::from_string_literal("Caller widened hard wire limits");
    auto boundary
        = "[1,[[5,false,[[[97],1,true],[[98],2,true],[[99],3,true],[[100],4,true]]],[6,4096,[]],[6,4096,[]],[6,4096,[]],[6,4096,[]]]]"sv;
    (void)TRY(Firefly::BridgeData::decode_wire(boundary, {})); // Exactly 16384 sparse slots.
    auto over
        = "[1,[[6,4,[[[48],1,true],[[49],2,true],[[50],3,true],[[51],4,true]]],[6,4096,[]],[6,4096,[]],[6,4096,[]],[6,4096,[]]]]"sv;
    if (!Firefly::BridgeData::decode_wire(over, {}).is_error())
        return Error::from_string_literal("Missing aggregate sparse-slot budget");
    auto function = run(u"(()=>42)"sv, source);
    if (function.is_error())
        return Error::from_string_literal("Missing function fixture");
    auto tokens = TRY(Firefly::BridgeData::capture(function.value(), source, {},
        [](JS::FunctionObject&, JS::Object*) -> ErrorOr<size_t> { return 0; }));
    if (!tokens.encode_wire({}).is_error())
        return Error::from_string_literal("Internal publication token escaped onto wire");
    auto full_value = run(u"Array(4095).fill(0)"sv, source);
    if (full_value.is_error())
        return Error::from_string_literal("Maximum node fixture failed");
    auto full = TRY(Firefly::BridgeData::capture(full_value.value(), source, {}));
    (void)TRY(Firefly::BridgeData::decode_wire(TRY(full.encode_wire({})), {}));
    auto full_string = run(u"String.fromCharCode(65535).repeat(65536)"sv, source);
    if (full_string.is_error())
        return Error::from_string_literal("Maximum UTF-16 fixture failed");
    auto full_text = TRY(Firefly::BridgeData::capture(full_string.value(), source, {}));
    auto full_bytes = TRY(full_text.encode_wire({}));
    (void)TRY(Firefly::BridgeData::decode_wire(full_bytes, {}));
    limits = {};
    limits.string_code_units = 65535;
    if (!full_text.encode_wire(limits).is_error()
        || !Firefly::BridgeData::decode_wire(full_bytes, limits).is_error())
        return Error::from_string_literal("UTF-16 exact boundary not enforced");
    // Rejections and a collection must leave both VMs' realm state usable.
    vm.heap().collect_garbage();
    TRY(check(u"wireCopy.left.n===42 && manualWire.__proto__===manualWire"sv, target));
    (void)TRY(Firefly::BridgeData::decode_wire(bytes, {}));
    outln("PASS wire graph round trip, UTF-16/cycles/holes/IEEE numbers, mutable/frozen copies, "
          "remote schema/refs/prototypes, exact budgets and malformed-byte recovery");
    return {};
}
