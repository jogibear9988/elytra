#include "BridgeDataTest.h"
#include "serialization/BridgeData.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibMain/Main.h>
#include <cstdio>
#include <iostream>
#include <string>

ErrorOr<int> ladybird_main(Main::Arguments args)
{
    if (args.argc != 2
        || (args.strings[1] != "encode"sv && args.strings[1] != "decode"sv
            && args.strings[1] != "checks"sv))
        return Error::from_string_literal("Expected checks, encode or decode");
    auto vm = JS::VM::create();
    auto initialized = JS::Realm::initialize_host_defined_realm(*vm, {}, {});
    if (initialized.is_error())
        return Error::from_string_literal("Cannot initialize wire test VM");
    auto context = initialized.release_value();
    ScopeGuard pop([&] { vm->pop_execution_context(); });
    auto& realm = *context->realm;
    auto run = [&](Utf16View text) -> ErrorOr<JS::Value> {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), realm);
        if (parsed.is_error())
            return Error::from_string_literal("Invalid wire process test script");
        GC::Root script { parsed.release_value() };
        auto value = vm->run(*script);
        if (value.is_error())
            return Error::from_string_literal("Wire process test script threw");
        return value.release_value();
    };

    if (args.strings[1] == "checks"sv) {
        auto initialized_target = JS::Realm::initialize_host_defined_realm(*vm, {}, {});
        if (initialized_target.is_error())
            return Error::from_string_literal("Cannot initialize target wire realm");
        auto target_context = initialized_target.release_value();
        ScopeGuard pop_target([&] { vm->pop_execution_context(); });
        TRY(check_bridge_wire(*vm, realm, *target_context->realm));
        return 0;
    }

    ByteString output;
    if (args.strings[1] == "encode"sv) {
        auto value = TRY(run(uR"JS(
            (()=>{const child={n:42},a={left:child,right:child,text:'\ud800\u0000\udfff',
                numbers:[-0,NaN,Infinity,-Infinity,Number.MIN_VALUE,Number.MAX_VALUE,1.2345678901234567],
                nil:null,u:undefined,yes:true};a.self=a;a.array=[,child,undefined];
                a.empty=Object.create(null);Object.defineProperty(a,'__proto__',{value:child,enumerable:false});return a})()
        )JS"sv));
        auto data = TRY(Firefly::BridgeData::capture(value, realm, {}));
        output = TRY(data.encode_wire({}));
    } else {
        // Read only max+1 bytes. Oversized input never reaches the JSON parser.
        std::string bytes(1024 * 1024 + 1, '\0');
        std::cin.read(bytes.data(), bytes.size());
        bytes.resize(static_cast<size_t>(std::cin.gcount()));
        auto decoded
            = TRY(Firefly::BridgeData::decode_wire(StringView(bytes.data(), bytes.size()), {}));
        auto value = decoded.materialize(realm);
        if (value.is_error())
            return Error::from_string_literal("Wire process materialization failed");
        auto root = GC::Root<JS::Value>::create(value.release_value(), SourceLocation::current());
        realm.global_object().define_direct_property(
            "wireData"_utf16, root.value(), JS::default_attributes);
        vm->heap().collect_garbage();
        auto checked = TRY(run(uR"JS(
            wireData.left.n===42 && wireData.self===wireData && wireData.left===wireData.right &&
            wireData.array.length===3 && !(0 in wireData.array) && wireData.array[1]===wireData.left &&
            2 in wireData.array && wireData.array[2]===undefined && wireData.yes===true && wireData.u===undefined && wireData.nil===null &&
            Object.is(wireData.numbers[0],-0) && Number.isNaN(wireData.numbers[1]) && wireData.numbers[2]===Infinity &&
            wireData.numbers[3]===-Infinity && wireData.numbers[4]===Number.MIN_VALUE && wireData.numbers[5]===Number.MAX_VALUE &&
            wireData.numbers[6]===1.2345678901234567 && wireData.text.length===3 && wireData.text.charCodeAt(0)===0xd800 &&
            wireData.text.charCodeAt(1)===0 && wireData.text.charCodeAt(2)===0xdfff &&
            Object.getPrototypeOf(wireData)===Object.prototype && Object.getPrototypeOf(wireData.array)===Array.prototype &&
            Object.getPrototypeOf(wireData.empty)===null && wireData.__proto__===wireData.left &&
            !Object.keys(wireData).includes('__proto__') && Object.isFrozen(wireData) && Object.isFrozen(wireData.left)
        )JS"sv));
        if (!checked.is_boolean() || !checked.as_bool())
            return Error::from_string_literal("Fresh VM data assertion failed");
        auto data = TRY(Firefly::BridgeData::capture(root.value(), realm, {}));
        output = TRY(data.encode_wire({}));
    }

    if (fwrite(output.characters(), 1, output.length(), stdout) != output.length())
        return Error::from_string_literal("Cannot write wire result");
    return 0;
}
