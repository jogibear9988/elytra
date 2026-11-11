#include "BridgeDataTest.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static JS::ThrowCompletionOr<JS::Value> run(JS::VM& vm, JS::Realm& realm, Utf16View text)
{
    auto parsed = JS::Script::parse(Utf16String::from_utf16(text), realm);
    if (parsed.is_error())
        return vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    GC::Root script { parsed.release_value() };
    return vm.run(*script);
}

ErrorOr<void> check_bridge_data(JS::VM& vm, JS::Realm& source, JS::Realm& target)
{
    auto check = [&](Utf16View code, JS::Realm& realm) -> ErrorOr<void> {
        auto answer = run(vm, realm, code);
        if (answer.is_error() || !answer.value().is_boolean() || !answer.value().as_bool())
            return Error::from_string_literal("Bridge data JS assertion failed");
        return {};
    };

    auto capture = [&](Utf16View code,
                       Firefly::BridgeData::Limits limits = {}) -> ErrorOr<Firefly::BridgeData> {
        auto value = run(vm, source, code);
        if (value.is_error())
            return Error::from_string_literal("Bridge source fixture threw");
        return Firefly::BridgeData::capture(value.value(), source, limits);
    };

    // IPC uses own data from real class instances (Code-OSS URI/VSBuffer shapes),
    // while contextBridge publication must continue rejecting those prototypes.
    auto ipc_value = run(vm, source, uR"JS(
        (()=>{ globalThis.ipcPrototypeReads=0;
          class URI { constructor(){this.scheme='vscode-file';this.path='/workbench'}
            get inherited(){ipcPrototypeReads++;throw 1} }
          class VSBuffer {constructor(buffer){this.buffer=buffer;this.byteLength=buffer.byteLength}}
          const uri=new URI(), view=new Uint8Array([1,2,255]);
          const value={uri,alias:uri,handle:new VSBuffer(view)};value.self=value;return value;
        })()
    )JS"sv);
    if (ipc_value.is_error()
        || !Firefly::BridgeData::capture(ipc_value.value(), source, {}).is_error())
        return Error::from_string_literal("Publication accepted custom instance");
    Firefly::BridgeData::Limits ipc_limits;
    ipc_limits.binary_bytes = 65536;
    auto ipc = TRY(Firefly::BridgeData::capture_ipc(ipc_value.value(), source, ipc_limits));
    auto ipc_wire = TRY(ipc.encode_wire(ipc_limits));
    auto ipc_decoded = TRY(Firefly::BridgeData::decode_wire(ipc_wire, ipc_limits));
    vm.heap().collect_garbage();
    auto ipc_copy
        = ipc_decoded.materialize(target, {}, Firefly::BridgeData::Materialization::Mutable);
    if (ipc_copy.is_error())
        return Error::from_string_literal("IPC instance materialization failed");
    target.global_object().define_direct_property(
        "ipcCopy"_utf16, ipc_copy.value(), JS::default_attributes);
    TRY(check(uR"JS(
        ipcCopy.self===ipcCopy && ipcCopy.uri===ipcCopy.alias &&
        Object.getPrototypeOf(ipcCopy.uri)===Object.prototype &&
        !('inherited' in ipcCopy.uri) && ipcCopy.uri.path==='/workbench' &&
        Object.getPrototypeOf(ipcCopy.handle)===Object.prototype &&
        ipcCopy.handle.buffer instanceof Uint8Array && ipcCopy.handle.buffer[2]===255
    )JS"sv,
        target));
    for (auto code :
        { u"new Proxy({},{ownKeys(){throw 1}})"sv, u"new (class {get own(){throw 1}})()"sv,
            u"(()=>{class C{};const c=new C;Object.defineProperty(c,'x',{get(){ipcPrototypeReads++;throw 1}});return c})()"sv,
            u"new (class {constructor(){this.fn=()=>1}})()"sv }) {
        auto bad = run(vm, source, code);
        // An inherited getter is not copied or executed, unlike an own accessor.
        bool inherited_only = code == u"new (class {get own(){throw 1}})()"sv;
        if (bad.is_error()
            || Firefly::BridgeData::capture_ipc(bad.value(), source, ipc_limits).is_error()
                == inherited_only)
            return Error::from_string_literal("IPC custom-instance rejection mismatch");
    }

    TRY(check(u"ipcPrototypeReads===0"sv, source));

    auto snapshot = TRY(capture(uR"JS(
        (() => {
            const shared={n:42};
            const value={a:shared,b:shared,zero:-0,nan:NaN,inf:Infinity,u:undefined,
                nil:null,yes:true,text:'\ud800\u0000x'};
            value.self=value;
            value.array=[1,,undefined,shared];
            value.empty=Object.create(null);
            value.empty.child=shared;
            Object.defineProperty(value,'__proto__',{value:{safe:true},enumerable:true});
            Object.defineProperty(value,'hidden',{value:'kept',enumerable:false});
            globalThis.sourceData=value;
            return value;
        })()
    )JS"sv));
    TRY(check(u"sourceData.a.n=99; delete globalThis.sourceData; true"sv, source));
    vm.heap().collect_garbage(); // snapshot must contain no source GC pointers.
    auto copied = snapshot.materialize(target);
    if (copied.is_error())
        return Error::from_string_literal("Bridge materialization failed");
    target.global_object().define_direct_property(
        "copied"_utf16, copied.value(), JS::default_attributes);
    TRY(check(uR"JS(
        copied.a.n===42 && copied.a===copied.b && copied.self===copied &&
        copied.array.length===4 && !(1 in copied.array) && (2 in copied.array) &&
        copied.array[2]===undefined && copied.array[3]===copied.a &&
        Object.is(copied.zero,-0) && Number.isNaN(copied.nan) && copied.inf===Infinity &&
        copied.nil===null && copied.yes===true && copied.u===undefined &&
        copied.text.length===3 && copied.text.charCodeAt(0)===0xd800 && copied.text.charCodeAt(1)===0 &&
        Object.getPrototypeOf(copied)===Object.prototype &&
        Object.getPrototypeOf(copied.array)===Array.prototype &&
        Object.getPrototypeOf(copied.empty)===null && copied.empty.child===copied.a &&
        Object.hasOwn(copied,'__proto__') && copied.__proto__.safe &&
        copied.hidden==='kept' && !Object.keys(copied).includes('hidden') &&
        [copied,copied.a,copied.array,copied.empty,copied.__proto__].every(Object.isFrozen) &&
        !({}).pagePoison && typeof sourceData==='undefined';
    )JS"sv,
        target));
    // A second reconstruction must not share mutable object identity with the
    // first. Also exercise the reverse direction into a poisoned page realm.
    auto again = snapshot.materialize(target);
    if (again.is_error() || &again.value().as_object() == &copied.value().as_object())
        return Error::from_string_literal("Bridge reconstruction reused object identity");
    TRY(check(
        u"globalThis.destinationSetterCalls=0; Object.defineProperty(Object.prototype,'answer',{set(){destinationSetterCalls++; throw 1},configurable:true}); true"sv,
        source));
    auto reverse = run(vm, target, u"({answer:7,nested:[true]})"sv);
    if (reverse.is_error())
        return Error::from_string_literal("Reverse source failed");
    if (!Firefly::BridgeData::capture(reverse.value(), source, {}).is_error())
        return Error::from_string_literal("Bridge accepted a foreign-realm container");
    auto reverse_data = TRY(Firefly::BridgeData::capture(reverse.value(), target, {}));
    auto reverse_copy = reverse_data.materialize(source);
    if (reverse_copy.is_error())
        return Error::from_string_literal("Reverse copy failed");
    source.global_object().define_direct_property(
        "reverseCopy"_utf16, reverse_copy.value(), JS::default_attributes);
    TRY(check(
        u"destinationSetterCalls===0 && Object.hasOwn(reverseCopy,'answer') && reverseCopy.answer===7 && reverseCopy.nested[0] && Object.isFrozen(reverseCopy.nested) && Object.getPrototypeOf(reverseCopy)===Object.prototype"sv,
        source));
    outln("PASS bridge graph copies preserve cycles/aliases/holes/numbers/UTF-16 and freeze "
          "target-realm containers");

    TRY(check(u"globalThis.traps=0; true"sv, source));
    for (auto text :
        {
            u"new Proxy({}, {ownKeys(){traps++;throw 1},getPrototypeOf(){traps++;throw 1},get(){traps++;throw 1}})"sv,
            u"Object.defineProperty({},'secret',{get(){traps++;throw 1}})"sv,
            u"({nested:Object.defineProperty({},'x',{set(v){traps++}})})"sv,
            u"({[Symbol('x')]:1})"sv,
            u"Symbol('x')"sv,
            u"1n"sv,
            u"(()=>42)"sv,
            u"Promise.resolve(1)"sv,
            u"document"sv,
            u"new Date()"sv,
            u"new Map()"sv,
            u"new Uint8Array(2)"sv,
            u"Object.create({privileged:true})"sv,
            u"new (class X {})"sv,
            u"Object.setPrototypeOf([],null)"sv,
            u"Object.assign([],{[Symbol('x')]:1})"sv,
            u"({ok:{},bad:window})"sv,
        }) {
        if (!capture(text).is_error())
            return Error::from_string_literal("Bridge accepted unsupported or active value");
    }

    TRY(check(u"traps===0"sv, source));
    outln("PASS bridge rejects proxies/accessors/symbols/functions/promises/host objects without "
          "executing traps");

    auto limits = Firefly::BridgeData::Limits {};
    limits.depth = 1;
    if (!capture(u"({a:{b:{}}})"sv, limits).is_error())
        return Error::from_string_literal("Missing depth budget");
    limits = {};
    limits.values = 2;
    if (!capture(u"[1,2]"sv, limits).is_error())
        return Error::from_string_literal("Missing value budget");
    limits = {};
    limits.properties = 1;
    if (!capture(u"({a:1,b:2})"sv, limits).is_error())
        return Error::from_string_literal("Missing property budget");
    limits = {};
    limits.string_code_units = 3;
    if (!capture(u"'abcd'"sv, limits).is_error() || !capture(u"({ab:'cd'})"sv, limits).is_error())
        return Error::from_string_literal("Missing cumulative string budget");
    limits = {};
    limits.array_length = 2;
    if (!capture(u"new Array(3)"sv, limits).is_error())
        return Error::from_string_literal("Missing sparse array budget");
    limits = {};
    limits.values = 0;
    if (!capture(u"null"sv, limits).is_error())
        return Error::from_string_literal("Missing zero budget rejection");
    limits = {};
    limits.properties = 1;
    if (!capture(u"({a:{b:1}})"sv, limits).is_error())
        return Error::from_string_literal("Missing cumulative property budget");
    limits = {};
    limits.values = 1;
    limits.properties = 1;
    limits.depth = 1;
    limits.string_code_units = 4;
    (void)TRY(capture(u"(()=>{const a={};a.self=a;return a})()"sv, limits));
    limits = {};
    limits.array_length = 2;
    (void)TRY(capture(u"new Array(2)"sv, limits));
    // Previous rejections must not poison later successful captures.
    (void)TRY(capture(u"({ok:true})"sv));
    vm.heap().collect_garbage();
    TRY(check(u"copied.a.n===42 && Object.isFrozen(copied)"sv, target));
    outln("PASS bridge depth/value/property/string/sparse-length budgets and GC after rejection");
    return {};
}

// Optional focused acceptance reads the real staged catalog; it never launches a window.
ErrorOr<void> check_large_bridge_catalog(
    JS::VM& vm, JS::Realm& source, JS::Realm& target, char const* filename)
{
    std::error_code failure;
    auto size = std::filesystem::file_size(filename, failure);
    if (failure || size > 4 * 1024 * 1024)
        return Error::from_string_literal("Missing or oversized catalog fixture");
    std::ifstream input(filename, std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(input), {});
    if (input.bad() || bytes.size() != size)
        return Error::from_string_literal("Cannot read catalog fixture");
    auto text = TRY(Utf16String::try_from_utf8(StringView(bytes.data(), bytes.size())));
    auto value = run(vm, source, text);
    if (value.is_error() || !value.value().is_object())
        return Error::from_string_literal("Catalog is not a JavaScript array");
    source.global_object().define_direct_property(
        "largeCatalog"_utf16, value.value(), JS::default_attributes);
    auto configuration = run(vm, source, u"({nls:{messages:largeCatalog}})"sv);
    if (configuration.is_error())
        return Error::from_string_literal("Catalog configuration construction failed");
    auto limits = Firefly::application_ipc_limits();
    if (!Firefly::BridgeData::capture_ipc(
            configuration.value(), source, Firefly::public_binary_limits())
            .is_error())
        return Error::from_string_literal("Default capture unexpectedly admitted full catalog");
    auto graph = TRY(Firefly::BridgeData::capture_ipc(configuration.value(), source, limits));
    auto wire = TRY(graph.encode_wire(limits));
    if (!Firefly::BridgeData::decode_wire(wire, Firefly::public_binary_limits()).is_error())
        return Error::from_string_literal("Default wire policy unexpectedly admitted full catalog");
    auto decoded = TRY(Firefly::BridgeData::decode_wire(wire, limits));
    vm.heap().collect_garbage();
    auto copy = decoded.materialize(target, {}, Firefly::BridgeData::Materialization::Mutable);
    if (copy.is_error())
        return Error::from_string_literal("Catalog materialization failed");
    target.global_object().define_direct_property(
        "largeCopy"_utf16, copy.value(), JS::default_attributes);
    auto original_json = run(vm, source, u"JSON.stringify(largeCatalog)"sv);
    if (original_json.is_error() || !original_json.value().is_string())
        return Error::from_string_literal("Source catalog comparison failed");
    auto expected = Utf16String::from_utf16(original_json.value().as_string().utf16_string_view());
    auto copied_json = run(vm, target, u"JSON.stringify(largeCopy.nls.messages)"sv);
    if (copied_json.is_error() || !copied_json.value().is_string()
        || copied_json.value().as_string().utf16_string_view() != expected.utf16_view())
        return Error::from_string_literal("Full catalog roundtrip changed translation data");
    outln("PASS complete staged translation catalog roundtrip, wire={} bytes", wire.length());
    return {};
}
