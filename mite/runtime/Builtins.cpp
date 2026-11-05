#include "ModuleLoader.h"
#include "ModulePaths.h"
#include "ModuleSource.h"
#include "BuiltinSources.h"
#include "BuiltinNames.h"
#include "ProcessHost.h"
#include "FileSystem.h"
#include "URL.h"
#include "Encoding.h"
#include "Network.h"
#include "DNS.h"
#include "HTTP.h"
#include "OS.h"
#include "Crypto.h"

#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/SyntheticModule.h>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace Mite {
using namespace ModulePaths;

struct BuiltinDefinition {
    std::string_view name;
    std::string_view source;
    std::span<std::string_view const> exports;
};
static constexpr std::string_view events_exports[] = {
    "EventEmitter", "errorMonitor", "captureRejectionSymbol", "captureRejections",
    "defaultMaxListeners", "listenerCount", "getEventListeners", "getMaxListeners", "setMaxListeners", "once"
};
static constexpr std::string_view process_exports[] = {
    "nextTick", "cwd", "chdir", "env", "pid", "ppid", "platform", "arch", "execPath", "argv", "argv0", "version", "versions", "release", "exit"
};
static constexpr std::string_view path_exports[] = {
    "resolve", "normalize", "isAbsolute", "join", "relative", "dirname", "basename", "extname", "format", "parse",
    "toNamespacedPath", "_makeLong", "sep", "delimiter", "win32", "posix"
};
static constexpr std::string_view buffer_exports[] = { "Buffer", "SlowBuffer" };
static constexpr std::string_view fs_exports[] = {
    "open", "openSync", "close", "closeSync", "read", "readSync", "write", "writeSync",
    "fstat", "fstatSync", "ftruncate", "ftruncateSync", "fsync", "fsyncSync", "fdatasync", "fdatasyncSync",
    "constants", "Stats", "Dirent", "promises", "readFile", "readFileSync", "writeFile", "writeFileSync",
    "appendFile", "appendFileSync", "access", "accessSync", "exists", "existsSync", "stat", "statSync", "lstat", "lstatSync",
    "mkdir", "mkdirSync", "rmdir", "rmdirSync", "unlink", "unlinkSync", "rename", "renameSync", "readdir", "readdirSync"
};
static constexpr std::string_view fs_promises_exports[] = {
    "open", "constants", "readFile", "writeFile", "appendFile", "access", "stat", "lstat", "mkdir", "rmdir", "unlink", "rename", "readdir"
};
static constexpr std::string_view url_exports[] = { "URL", "URLSearchParams", "fileURLToPath", "fileURLToPathBuffer", "pathToFileURL", "domainToASCII", "domainToUnicode", "urlToHttpOptions" };
static constexpr std::string_view util_exports[] = { "TextEncoder", "TextDecoder" };
static constexpr std::string_view stream_exports[] = {
    "Stream", "Readable", "Writable", "Duplex", "Transform", "PassThrough", "pipeline", "finished", "compose", "destroy", "addAbortSignal",
    "isDestroyed", "isDisturbed", "isErrored", "isReadable", "isWritable", "getDefaultHighWaterMark", "setDefaultHighWaterMark", "promises"
};
static constexpr std::string_view stream_promises_exports[] = { "pipeline", "finished" };
static constexpr std::string_view string_decoder_exports[] = { "StringDecoder" };
static constexpr std::string_view net_exports[] = { "Socket", "Server", "createServer", "connect", "createConnection", "isIP", "isIPv4", "isIPv6" };
static constexpr std::string_view dns_exports[] = { "lookup", "promises", "setDefaultResultOrder", "getDefaultResultOrder", "ADDRCONFIG", "V4MAPPED", "ALL" };
static constexpr std::string_view dns_promises_exports[] = { "lookup", "setDefaultResultOrder", "getDefaultResultOrder" };
static constexpr std::string_view http_exports[] = { "Server", "ServerResponse", "OutgoingMessage", "IncomingMessage", "createServer", "METHODS", "STATUS_CODES", "validateHeaderName", "validateHeaderValue", "maxHeaderSize" };
static constexpr std::string_view os_exports[] = { "arch", "availableParallelism", "endianness", "freemem", "homedir", "hostname", "loadavg", "platform", "release", "tmpdir", "totalmem", "type", "uptime", "version", "machine", "cpus", "networkInterfaces", "userInfo", "getPriority", "setPriority", "constants", "EOL", "devNull" };
static constexpr std::string_view crypto_exports[] = { "randomBytes", "randomUUID", "createHash", "Hash" };
static constexpr BuiltinDefinition definitions[] = {
    { "crypto", { MITE_CRYPTO_SOURCE, sizeof(MITE_CRYPTO_SOURCE) - 1 }, crypto_exports },
    { "os", { MITE_OS_SOURCE, sizeof(MITE_OS_SOURCE) - 1 }, os_exports },
    { "http", { MITE_HTTP_SOURCE, sizeof(MITE_HTTP_SOURCE) - 1 }, http_exports },
    { "dns", { MITE_DNS_SOURCE, sizeof(MITE_DNS_SOURCE) - 1 }, dns_exports },
    { "dns/promises", {}, dns_promises_exports },
    { "net", { MITE_NET_SOURCE, sizeof(MITE_NET_SOURCE) - 1 }, net_exports },
    { "stream", { MITE_STREAM_SOURCE, sizeof(MITE_STREAM_SOURCE) - 1 }, stream_exports },
    { "stream/promises", {}, stream_promises_exports },
    { "string_decoder", { MITE_STRING_DECODER_SOURCE, sizeof(MITE_STRING_DECODER_SOURCE) - 1 }, string_decoder_exports },
    { "util", { MITE_UTIL_SOURCE, sizeof(MITE_UTIL_SOURCE) - 1 }, util_exports },
    { "url", { MITE_URL_SOURCE, sizeof(MITE_URL_SOURCE) - 1 }, url_exports },
    { "fs", { MITE_FS_SOURCE, sizeof(MITE_FS_SOURCE) - 1 }, fs_exports },
    { "fs/promises", {}, fs_promises_exports },
    { "buffer", { MITE_BUFFER_SOURCE, sizeof(MITE_BUFFER_SOURCE) - 1 }, buffer_exports },
    { "events", { MITE_EVENTS_SOURCE, sizeof(MITE_EVENTS_SOURCE) - 1 }, events_exports },
    { "process", {}, process_exports },
    { "path", { MITE_PATH_SOURCE, sizeof(MITE_PATH_SOURCE) - 1 }, path_exports },
    { "path/posix", {}, path_exports },
    { "path/win32", {}, path_exports }
};
static BuiltinDefinition const* definition_for(std::string const& name)
{
    for (auto const& definition : definitions) {
        if (definition.name == name)
            return &definition;
    }
    return nullptr;
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> ModuleLoader::builtin(std::string const& name)
{
    if (auto found = m_builtins.find(name); found != m_builtins.end())
        return *found->second;
    if (!known_builtin(name))
        return error("ERR_UNKNOWN_BUILTIN_MODULE", "No such builtin module: node:" + name);
    auto const* definition = definition_for(name);
    if (!definition)
        return error("ERR_MITE_UNSUPPORTED_BUILTIN", "Builtin module is not implemented: node:" + name);
    if (name == "process") {
        m_builtins.emplace(name, m_process);
        return *m_process;
    }
    if (name == "fs/promises" || name == "stream/promises" || name == "dns/promises") {
        auto owner = TRY(builtin(name.substr(0, name.find('/'))));
        auto promises = TRY(owner->get("promises"_utf16));
        if (!promises.is_object()) return error("ERR_MITE_BUILTIN_INITIALIZATION", "Builtin promises alias must be an object");
        m_builtins.emplace(name, GC::Root<JS::Object> { promises.as_object() });
        return promises.as_object();
    }
    if (name == "path/posix" || name == "path/win32") {
        auto path = TRY(builtin("path"));
        auto alias = TRY(path->get(name == "path/posix" ? "posix"_utf16 : "win32"_utf16));
        if (!alias.is_object())
            return error("ERR_MITE_BUILTIN_INITIALIZATION", "Path alias must be an object");
        m_builtins.emplace(name, GC::Root<JS::Object> { alias.as_object() });
        return alias.as_object();
    }
    auto source = Utf16String::from_utf8(StringView(definition->source.data(), definition->source.size()));
    auto identity = "node:" + name;
    auto parsed = JS::Script::parse(source, m_realm, view(identity));
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    GC::Root script { parsed.release_value() };
    auto factory = TRY(m_vm.run(*script));
    GC::Root host { JS::Object::create(m_realm, nullptr) };
    if (name == "stream" || name == "string_decoder") {
        host->define_native_function(m_realm, "require"_utf16, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string()) return error("ERR_INVALID_ARG_TYPE", "Internal builtin name must be a string", true);
            auto encoded = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
            auto name = string(encoded);
            if (!name.starts_with("node:")) return error("ERR_MITE_UNSUPPORTED_BUILTIN", "Unexpected portable runtime dependency");
            return TRY(builtin(name.substr(5)));
        }, 1, JS::default_attributes);
        if (name == "string_decoder") {
            auto native = create_encoding_host(m_realm);
            host->define_direct_property("byteView"_utf16, TRY(native->get("byteView"_utf16)), JS::default_attributes);
        }
    }
    if (name == "crypto") {
        host->define_direct_property("stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        host->define_direct_property("encoding"_utf16, create_encoding_host(m_realm), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property("Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
        host->define_direct_property("native"_utf16, create_crypto_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
    }
    if (name == "os") {
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property("Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
        host->define_direct_property("native"_utf16, TRY(create_os_host(m_realm)), JS::default_attributes);
        host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
    }
    if (name == "http") {
        host->define_direct_property("native"_utf16, create_http_host(m_realm), JS::default_attributes);
        host->define_direct_property("net"_utf16, TRY(builtin("net")), JS::default_attributes);
        host->define_direct_property("stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property("Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }
    if (name == "dns") {
        host->define_direct_property("native"_utf16, create_dns_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
        auto url = TRY(builtin("url"));
        host->define_direct_property("domainToASCII"_utf16, TRY(url->get("domainToASCII"_utf16)), JS::default_attributes);
    }
    if (name == "net") {
        host->define_direct_property("dns"_utf16, TRY(builtin("dns")), JS::default_attributes);
        host->define_direct_property("native"_utf16, create_network_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
        host->define_direct_property("stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        auto events = TRY(builtin("events"));
        host->define_direct_property("EventEmitter"_utf16, TRY(events->get("EventEmitter"_utf16)), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property("Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }
    if (name == "util") host->define_direct_property("native"_utf16, create_encoding_host(m_realm), JS::default_attributes);
    if (name == "url") {
        host->define_direct_property("native"_utf16, create_url_host(m_realm), JS::default_attributes);
        host->define_direct_property("path"_utf16, TRY(builtin("path")), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property("Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }
    if (name == "fs") {
        auto url = TRY(builtin("url"));
        host->define_direct_property("URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
        host->define_direct_property("fileURLToPath"_utf16, TRY(url->get("fileURLToPath"_utf16)), JS::default_attributes);
        host->define_direct_property("native"_utf16, create_filesystem_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
        auto events = TRY(builtin("events"));
        host->define_direct_property("EventEmitter"_utf16, TRY(events->get("EventEmitter"_utf16)), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property("Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }
    host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
    host->define_direct_property("platform"_utf16,
        JS::PrimitiveString::create(m_vm, Utf16String::from_utf8(StringView(native_platform(), strlen(native_platform())))), JS::default_attributes);
    host->define_native_function(m_realm, "warn"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto name = TRY(vm.argument(0).to_utf16_string(vm));
        auto message = TRY(vm.argument(1).to_utf16_string(vm));
        auto formatted = Utf16String::formatted("{}: {}\n", name, message);
        auto line = MUST(formatted.utf16_view().to_byte_string());
        fwrite(line.characters(), 1, line.length(), stderr);
        return JS::js_undefined();
    }, 2, JS::default_attributes);
    auto value = TRY(JS::call(m_vm, factory, JS::js_undefined(), JS::Value(host.ptr())));
    if (!value.is_object())
        return error("ERR_MITE_BUILTIN_INITIALIZATION", "Builtin factory returned a non-object");
    m_builtins.emplace(name, GC::Root<JS::Object> { value.as_object() });
    return value.as_object();
}

JS::ThrowCompletionOr<void> ModuleLoader::install_globals()
{
    auto buffer = TRY(builtin("buffer"));
    auto constructor = TRY(buffer->get("Buffer"_utf16));
    m_realm.global_object().define_direct_property("Buffer"_utf16, constructor, JS::default_attributes);
    auto url = TRY(builtin("url"));
    m_realm.global_object().define_direct_property("URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
    m_realm.global_object().define_direct_property("URLSearchParams"_utf16, TRY(url->get("URLSearchParams"_utf16)), JS::default_attributes);
    auto util = TRY(builtin("util"));
    m_realm.global_object().define_direct_property("TextEncoder"_utf16, TRY(util->get("TextEncoder"_utf16)), JS::default_attributes);
    m_realm.global_object().define_direct_property("TextDecoder"_utf16, TRY(util->get("TextDecoder"_utf16)), JS::default_attributes);
    return {};
}

JS::ThrowCompletionOr<GC::Ref<JS::Module>> ModuleLoader::builtin_module(std::string const& name)
{
    auto identity = "node:" + name;
    if (auto found = m_modules.find(identity); found != m_modules.end())
        return *found->second;
    // Initialize before defining the fixed ESM export list. Future native modules
    // share this registry and the same object returned by CommonJS require.
    TRY(builtin(name));
    if (!m_builtin_bridge) {
        auto bridge = JS::NativeFunction::create(m_realm, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return error("ERR_INVALID_ARG_TYPE", "Builtin name must be a string", true);
            auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
            return TRY(builtin(string(bytes)));
        }, 1, "getBuiltin"_utf16);
        m_builtin_bridge = JS::SyntheticModule::create_default_export_synthetic_module(m_realm, bridge, "mite:internal/builtin").ptr();
    }
    auto quoted = AK::JsonValue(view(name)).serialized();
    StringBuilder source;
    source.appendff("import get from 'mite:internal/builtin'; const value = get({}); export {{ value as default }};", quoted);
    auto const* definition = definition_for(name);
    VERIFY(definition);
    for (auto export_name : definition->exports) {
        StringView exported { export_name.data(), export_name.size() };
        source.appendff("export const {} = value.{};", exported, exported);
    }
    GC::Root metadata { m_vm.heap().allocate<ModuleSource>(identity) };
    auto parsed = JS::SourceTextModule::parse(Utf16String::from_utf8(source.string_view()), m_realm, view(identity), {}, metadata.ptr());
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    auto module = parsed.release_value();
    m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
    m_builtin_shims.insert(module.ptr());
    return module;
}

}
