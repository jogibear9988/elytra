/**
 * Constructs the supported Node-compatible builtin modules. Native host objects are injected
 * into compatibility factories and cached per loader so ESM and CommonJS observe the same
 * instances.
 */

#include "BuiltinSources.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "modules/BuiltinNames.h"
#include "modules/ModuleLoader.h"
#include "modules/ModulePaths.h"
#include "modules/ModuleSource.h"
#include "platform/compression/Codec.h"
#include "platform/compression/Inflater.h"
#include "platform/crypto/Crypto.h"
#include "platform/filesystem/FileSystem.h"
#include "platform/network/DNS.h"
#include "platform/network/HTTP.h"
#include "platform/network/Network.h"
#include "platform/network/tls/Client.h"
#include "platform/process/ChildProcess.h"
#include "platform/process/ProcessHost.h"
#include "platform/process/Signals.h"
#include "platform/process/Timing.h"
#include "platform/system/Inspector.h"
#include "platform/system/OS.h"
#include "platform/text/Encoding.h"
#include "platform/text/URL.h"
#include "platform/util/Types.h"

#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/SyntheticModule.h>
#include <algorithm>
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

static constexpr std::string_view assert_exports[] = { "AssertionError", "ok", "fail", "equal",
    "notEqual", "deepEqual", "notDeepEqual", "deepStrictEqual", "notDeepStrictEqual", "strictEqual",
    "notStrictEqual", "throws", "rejects", "doesNotThrow", "doesNotReject", "ifError", "match",
    "doesNotMatch", "strict", "Assert", "CallTracker", "partialDeepStrictEqual" };
static constexpr std::string_view readline_exports[] = { "Interface", "createInterface",
    "clearLine", "clearScreenDown", "cursorTo", "moveCursor", "emitKeypressEvents" };
static constexpr std::string_view diagnostics_channel_exports[]
    = { "channel", "hasSubscribers", "subscribe", "tracingChannel", "unsubscribe", "Channel" };
static constexpr std::string_view events_exports[] = { "EventEmitter", "errorMonitor",
    "captureRejectionSymbol", "captureRejections", "defaultMaxListeners", "listenerCount",
    "getEventListeners", "getMaxListeners", "setMaxListeners", "once" };
static constexpr std::string_view process_exports[] = { "nextTick", "cwd", "chdir", "env", "pid",
    "ppid", "platform", "arch", "execPath", "execArgv", "argv", "argv0", "version", "versions",
    "release", "exit", "hrtime", "uptime", "cpuUsage", "stdout", "stderr" };
static constexpr std::string_view path_exports[] = { "resolve", "normalize", "isAbsolute", "join",
    "relative", "dirname", "basename", "extname", "format", "parse", "toNamespacedPath",
    "_makeLong", "sep", "delimiter", "win32", "posix" };
static constexpr std::string_view buffer_exports[] = { "Buffer", "SlowBuffer", "Blob", "File" };
static constexpr std::string_view fs_exports[] = { "open", "openSync", "close", "closeSync", "read",
    "readSync", "write", "writeSync", "fstat", "fstatSync", "ftruncate", "ftruncateSync", "fsync",
    "fsyncSync", "fdatasync", "fdatasyncSync", "constants", "Stats", "Dirent", "promises",
    "readFile", "readFileSync", "writeFile", "writeFileSync", "appendFile", "appendFileSync",
    "access", "accessSync", "exists", "existsSync", "stat", "statSync", "lstat", "lstatSync",
    "mkdir", "mkdirSync", "chmod", "chmodSync", "mkdtemp", "mkdtempSync", "rm", "rmSync", "rmdir",
    "rmdirSync", "unlink", "unlinkSync", "rename", "renameSync", "readdir", "readdirSync",
    "WriteStream", "createWriteStream", "watchFile", "unwatchFile", "watch", "realpath",
    "realpathSync", "copyFile", "copyFileSync", "link", "linkSync", "readlink", "readlinkSync",
    "symlink", "symlinkSync", "utimes", "utimesSync", "opendir", "opendirSync", "Dir", "cp",
    "ReadStream", "createReadStream" };
static constexpr std::string_view fs_promises_exports[]
    = { "open", "constants", "readFile", "writeFile", "appendFile", "access", "stat", "lstat",
          "mkdir", "chmod", "mkdtemp", "rm", "rmdir", "unlink", "rename", "readdir", "realpath",
          "copyFile", "link", "readlink", "symlink", "utimes", "opendir", "cp" };
static constexpr std::string_view async_hooks_exports[] = { "AsyncLocalStorage", "AsyncResource",
    "executionAsyncId", "triggerAsyncId", "executionAsyncResource", "createHook" };
static constexpr std::string_view querystring_exports[]
    = { "parse", "decode", "stringify", "encode", "escape", "unescape", "unescapeBuffer" };
static constexpr std::string_view url_exports[]
    = { "URL", "URLSearchParams", "parse", "Url", "fileURLToPath", "fileURLToPathBuffer",
          "pathToFileURL", "domainToASCII", "domainToUnicode", "urlToHttpOptions" };
static constexpr std::string_view util_exports[] = { "TextEncoder", "TextDecoder", "promisify",
    "inherits", "types", "debuglog", "debug", "inspect", "format", "formatWithOptions" };
static constexpr std::string_view util_types_exports[]
    = { "isArrayBuffer", "isSharedArrayBuffer", "isAnyArrayBuffer", "isArrayBufferView",
          "isTypedArray", "isUint8Array", "isDataView", "isProxy", "isPromise" };
static constexpr std::string_view console_exports[]
    = { "Console", "log", "info", "debug", "warn", "error", "dir", "assert", "table", "time",
          "timeEnd", "timeLog", "count", "countReset", "group", "groupCollapsed", "groupEnd",
          "clear", "trace", "dirxml", "profile", "profileEnd", "timeStamp" };
static constexpr std::string_view module_exports[]
    = { "createRequire", "isBuiltin", "builtinModules", "registerHooks" };
static constexpr std::string_view perf_hooks_exports[] = { "performance" };
static constexpr std::string_view stream_exports[] = { "Stream", "Readable", "Writable", "Duplex",
    "Transform", "PassThrough", "pipeline", "finished", "compose", "destroy", "addAbortSignal",
    "isDestroyed", "isDisturbed", "isErrored", "isReadable", "isWritable",
    "getDefaultHighWaterMark", "setDefaultHighWaterMark", "promises" };
static constexpr std::string_view stream_promises_exports[] = { "pipeline", "finished" };
static constexpr std::string_view web_streams_exports[]
    = { "DecompressionStream", "ByteLengthQueuingStrategy", "CountQueuingStrategy",
          "ReadableByteStreamController", "ReadableStream", "ReadableStreamBYOBReader",
          "ReadableStreamBYOBRequest", "ReadableStreamDefaultController",
          "ReadableStreamDefaultReader", "TransformStream", "TransformStreamDefaultController",
          "WritableStream", "WritableStreamDefaultController", "WritableStreamDefaultWriter" };
static constexpr std::string_view string_decoder_exports[] = { "StringDecoder" };
static constexpr std::string_view net_exports[] = { "Socket", "Server", "createServer", "connect",
    "createConnection", "isIP", "isIPv4", "isIPv6" };
static constexpr std::string_view dns_exports[] = { "lookup", "promises", "setDefaultResultOrder",
    "getDefaultResultOrder", "ADDRCONFIG", "V4MAPPED", "ALL" };
static constexpr std::string_view dns_promises_exports[]
    = { "lookup", "setDefaultResultOrder", "getDefaultResultOrder" };
static constexpr std::string_view http_exports[] = { "Server", "ServerResponse", "OutgoingMessage",
    "IncomingMessage", "ClientRequest", "request", "get", "createServer", "METHODS", "STATUS_CODES",
    "validateHeaderName", "validateHeaderValue", "maxHeaderSize" };
static constexpr std::string_view inspector_exports[]
    = { "url", "open", "close", "waitForDebugger", "Session" };
static constexpr std::string_view os_exports[] = { "arch", "availableParallelism", "endianness",
    "freemem", "homedir", "hostname", "loadavg", "platform", "release", "tmpdir", "totalmem",
    "type", "uptime", "version", "machine", "cpus", "networkInterfaces", "userInfo", "getPriority",
    "setPriority", "constants", "EOL", "devNull" };
static constexpr std::string_view zlib_exports[] = { "constants", "DeflateRaw", "createDeflateRaw",
    "deflateRaw", "deflateRawSync", "InflateRaw", "createInflateRaw", "inflateRaw",
    "inflateRawSync", "Deflate", "createDeflate", "deflate", "deflateSync", "Inflate",
    "createInflate", "inflate", "inflateSync", "Gzip", "createGzip", "gzip", "gzipSync", "Gunzip",
    "createGunzip", "gunzip", "gunzipSync", "createUnzip", "Unzip", "createBrotliCompress",
    "createBrotliDecompress", "createZstdCompress", "createZstdDecompress", "brotliCompress",
    "brotliCompressSync", "brotliDecompress", "brotliDecompressSync", "zstdCompress",
    "zstdCompressSync", "zstdDecompress", "zstdDecompressSync", "crc32" };
static constexpr std::string_view crypto_exports[] = { "randomBytes", "randomUUID", "randomInt",
    "getHashes", "createHash", "Hash", "createHmac", "Hmac", "timingSafeEqual", "webcrypto", "getRandomValues",
    "X509Certificate", "createPublicKey", "verify" };
static constexpr std::string_view child_process_exports[]
    = { "spawn", "execFile", "exec", "fork", "ChildProcess" };
static constexpr std::string_view timers_exports[] = { "setTimeout", "setInterval", "setImmediate",
    "clearTimeout", "clearInterval", "clearImmediate" };
static constexpr std::string_view timers_promises_exports[] = { "setTimeout", "setImmediate" };
static constexpr std::string_view tls_exports[]
    = { "connect", "TLSSocket", "createServer", "createSecureContext", "getCACertificates" };
static constexpr std::string_view https_exports[]
    = { "request", "get", "createServer", "Server", "Agent" };
static constexpr BuiltinDefinition definitions[] = {
    { "diagnostics_channel",
        { MITE_DIAGNOSTICS_CHANNEL_SOURCE, sizeof(MITE_DIAGNOSTICS_CHANNEL_SOURCE) - 1 },
        diagnostics_channel_exports },
    { "module", { MITE_MODULE_SOURCE, sizeof(MITE_MODULE_SOURCE) - 1 }, module_exports },
    { "perf_hooks", { MITE_PERF_HOOKS_SOURCE, sizeof(MITE_PERF_HOOKS_SOURCE) - 1 },
        perf_hooks_exports },
    { "timers/promises", { MITE_TIMERS_PROMISES_SOURCE, sizeof(MITE_TIMERS_PROMISES_SOURCE) - 1 },
        timers_promises_exports },
    { "timers", {}, timers_exports },
    { "readline", { MITE_READLINE_SOURCE, sizeof(MITE_READLINE_SOURCE) - 1 }, readline_exports },
    { "zlib", { MITE_ZLIB_SOURCE, sizeof(MITE_ZLIB_SOURCE) - 1 }, zlib_exports },
    { "crypto", { MITE_CRYPTO_SOURCE, sizeof(MITE_CRYPTO_SOURCE) - 1 }, crypto_exports },
    { "child_process", { MITE_CHILD_PROCESS_SOURCE, sizeof(MITE_CHILD_PROCESS_SOURCE) - 1 },
        child_process_exports },
    { "inspector", { MITE_INSPECTOR_SOURCE, sizeof(MITE_INSPECTOR_SOURCE) - 1 },
        inspector_exports },
    { "os", { MITE_OS_SOURCE, sizeof(MITE_OS_SOURCE) - 1 }, os_exports },
    { "tls", { MITE_TLS_SOURCE, sizeof(MITE_TLS_SOURCE) - 1 }, tls_exports },
    { "https", { MITE_HTTPS_SOURCE, sizeof(MITE_HTTPS_SOURCE) - 1 }, https_exports },
    { "http", { MITE_HTTP_SOURCE, sizeof(MITE_HTTP_SOURCE) - 1 }, http_exports },
    { "dns", { MITE_DNS_SOURCE, sizeof(MITE_DNS_SOURCE) - 1 }, dns_exports },
    { "dns/promises", {}, dns_promises_exports },
    { "net", { MITE_NET_SOURCE, sizeof(MITE_NET_SOURCE) - 1 }, net_exports },
    { "stream", { MITE_STREAM_SOURCE, sizeof(MITE_STREAM_SOURCE) - 1 }, stream_exports },
    { "stream/promises", {}, stream_promises_exports }, { "stream/web", {}, web_streams_exports },
    { "string_decoder", { MITE_STRING_DECODER_SOURCE, sizeof(MITE_STRING_DECODER_SOURCE) - 1 },
        string_decoder_exports },
    { "util", { MITE_UTIL_SOURCE, sizeof(MITE_UTIL_SOURCE) - 1 }, util_exports },
    { "util/types", { MITE_UTIL_TYPES_SOURCE, sizeof(MITE_UTIL_TYPES_SOURCE) - 1 },
        util_types_exports },
    { "console", { MITE_CONSOLE_SOURCE, sizeof(MITE_CONSOLE_SOURCE) - 1 }, console_exports },
    { "async_hooks", { MITE_ASYNC_HOOKS_SOURCE, sizeof(MITE_ASYNC_HOOKS_SOURCE) - 1 },
        async_hooks_exports },
    { "querystring", { MITE_QUERYSTRING_SOURCE, sizeof(MITE_QUERYSTRING_SOURCE) - 1 },
        querystring_exports },
    { "url", { MITE_URL_SOURCE, sizeof(MITE_URL_SOURCE) - 1 }, url_exports },
    { "fs", { MITE_FS_SOURCE, sizeof(MITE_FS_SOURCE) - 1 }, fs_exports },
    { "fs/promises", {}, fs_promises_exports },
    { "buffer", { MITE_BUFFER_SOURCE, sizeof(MITE_BUFFER_SOURCE) - 1 }, buffer_exports },
    { "assert", { MITE_ASSERT_SOURCE, sizeof(MITE_ASSERT_SOURCE) - 1 }, assert_exports },
    { "assert/strict", {}, assert_exports },
    { "events", { MITE_EVENTS_SOURCE, sizeof(MITE_EVENTS_SOURCE) - 1 }, events_exports },
    { "process", {}, process_exports },
    { "path", { MITE_PATH_SOURCE, sizeof(MITE_PATH_SOURCE) - 1 }, path_exports },
    { "path/posix", {}, path_exports }, { "path/win32", {}, path_exports }
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
        return error(
            "ERR_MITE_UNSUPPORTED_BUILTIN", "Builtin module is not implemented: node:" + name);
    if (name == "assert/strict") {
        auto root = TRY(builtin("assert"));
        auto strict = TRY(root->get("strict"_utf16));
        m_builtins.emplace(name, GC::Root<JS::Object> { strict.as_object() });
        return strict.as_object();
    }

    if (name == "timers") {
        GC::Root result { JS::Object::create(m_realm, m_realm.intrinsics().object_prototype()) };
        for (auto name : timers_exports) {
            auto key = Utf16String::from_utf8(StringView(name.data(), name.size()));
            result->define_direct_property(
                key, m_realm.global_object().get_without_side_effects(key), JS::default_attributes);
        }

        m_builtins.emplace(name, GC::Root<JS::Object> { *result });
        return *result;
    }

    if (name == "stream/web") {
        m_builtins.emplace(name, m_web_streams);
        return *m_web_streams;
    }

    if (name == "process") {
        m_builtins.emplace(name, m_process);
        return *m_process;
    }

    if (name == "fs/promises" || name == "stream/promises" || name == "dns/promises") {
        auto owner = TRY(builtin(name.substr(0, name.find('/'))));
        auto promises = TRY(owner->get("promises"_utf16));
        if (!promises.is_object())
            return error(
                "ERR_MITE_BUILTIN_INITIALIZATION", "Builtin promises alias must be an object");
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

    auto result = TRY(construct_builtin(name));
    m_builtins.emplace(name, GC::Root<JS::Object> { result });
    return result;
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> ModuleLoader::construct_builtin(std::string const& name)
{
    auto const* definition = definition_for(name);
    if (!definition || definition->source.empty())
        return error("ERR_MITE_UNSUPPORTED_BUILTIN", "No factory for builtin: " + name);
    auto source
        = Utf16String::from_utf8(StringView(definition->source.data(), definition->source.size()));
    auto identity = "node:" + name;
    auto parsed = JS::Script::parse(source, m_realm, view(identity));
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    GC::Root script { parsed.release_value() };
    auto factory = TRY(m_vm.run(*script));
    GC::Root host { JS::Object::create(m_realm, nullptr) };
    // This private realm-local symbol state preserves named callback results and
    // stays separate from the mutable public util/fs/dns facades.
    if (name == "util" || name == "fs" || name == "dns")
        host->define_direct_property(
            "promisifyState"_utf16, m_promisify_state.ptr(), JS::default_attributes);
    if (name == "perf_hooks")
        install_performance_timing(m_realm, *host);
    if (name == "module") {
        install_module_utilities(*host);
        host->define_direct_property("path"_utf16, TRY(builtin("path")), JS::default_attributes);
        host->define_direct_property("url"_utf16, TRY(builtin("url")), JS::default_attributes);
        GC::RootVector<JS::Value> names;
        std::vector<std::string_view> sorted;
        for (auto const& candidate : definitions)
            sorted.push_back(candidate.name);
        std::sort(sorted.begin(), sorted.end());
        for (auto candidate : sorted)
            names.append(JS::PrimitiveString::create(
                m_vm, Utf16String::from_utf8(StringView(candidate.data(), candidate.size()))));
        host->define_direct_property(
            "builtinModules"_utf16, JS::Array::create_from(m_realm, names), JS::default_attributes);
        host->define_native_function(
            m_realm, "isBuiltin"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_string())
                    return JS::Value(false);
                auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                auto name = string(bytes);
                if (name.starts_with("node:"))
                    name.erase(0, 5);
                return JS::Value(definition_for(name) != nullptr);
            },
            1, JS::default_attributes);
    }

    if (name == "events")
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
    if (name == "timers/promises") {
        host->define_direct_property(
            "timers"_utf16, TRY(builtin("timers")), JS::default_attributes);
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
    }

    if (name == "child_process") {
        host->define_direct_property(
            "childIpc"_utf16, JS::Value(m_fork_child), JS::default_attributes);
        host->define_direct_property("native"_utf16,
            create_child_process_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
        host->define_direct_property(
            "events"_utf16, TRY(builtin("events")), JS::default_attributes);
        host->define_direct_property(
            "stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
        auto url = TRY(builtin("url"));
        host->define_direct_property(
            "URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "fileURLToPath"_utf16, TRY(url->get("fileURLToPath"_utf16)), JS::default_attributes);
        auto os = TRY(builtin("os"));
        auto constants = TRY(os->get("constants"_utf16));
        host->define_direct_property("signals"_utf16,
            TRY(constants.as_object().get("signals"_utf16)), JS::default_attributes);
    }

    if (name == "stream") {
        host->define_direct_property(
            "webStreams"_utf16, m_web_streams.ptr(), JS::default_attributes);
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
    }

    if (name == "stream" || name == "string_decoder") {
        host->define_native_function(m_realm, "require"_utf16,
            GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(*this)),
                [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto* self = &captured_self.get();

                    if (!vm.argument(0).is_string())
                        return self->error(
                            "ERR_INVALID_ARG_TYPE", "Internal builtin name must be a string", true);
                    auto encoded
                        = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                    auto name = string(encoded);
                    if (!name.starts_with("node:"))
                        return self->error("ERR_MITE_UNSUPPORTED_BUILTIN",
                            "Unexpected portable runtime dependency");
                    return TRY(self->builtin(name.substr(5)));
                }),
            1, JS::default_attributes);
        if (name == "string_decoder") {
            auto native = create_encoding_host(m_realm);
            host->define_direct_property(
                "byteView"_utf16, TRY(native->get("byteView"_utf16)), JS::default_attributes);
        }
    }

    if (name == "readline") {
        host->define_direct_property(
            "events"_utf16, TRY(builtin("events")), JS::default_attributes);
        host->define_direct_property(
            "decoder"_utf16, TRY(builtin("string_decoder")), JS::default_attributes);
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
        host->define_direct_property(
            "tick"_utf16, TRY(m_process->get("nextTick"_utf16)), JS::default_attributes);
        host->define_direct_property("schedule"_utf16,
            TRY(m_realm.global_object().get("setImmediate"_utf16)), JS::default_attributes);
    }

    if (name == "zlib") {
        host->define_direct_property("schedule"_utf16,
            TRY(m_realm.global_object().get("setImmediate"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "nextTick"_utf16, TRY(m_process->get("nextTick"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "native"_utf16, create_zlib_host(m_realm), JS::default_attributes);
    }

    if (name == "crypto") {
        host->define_direct_property("DOMException"_utf16,
            TRY(m_realm.global_object().get("DOMException"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        host->define_direct_property(
            "encoding"_utf16, create_encoding_host(m_realm), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
        host->define_direct_property("native"_utf16,
            create_crypto_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
    }

    if (name == "os") {
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "native"_utf16, TRY(create_os_host(m_realm)), JS::default_attributes);
        host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
    }

    if (name == "http") {
        m_http_client_hooks = JS::Object::create(m_realm, nullptr);
        host->define_direct_property(
            "clientHooks"_utf16, m_http_client_hooks.ptr(), JS::default_attributes);
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
        auto url = TRY(builtin("url"));
        host->define_direct_property(
            "URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "native"_utf16, create_http_host(m_realm), JS::default_attributes);
        host->define_direct_property("net"_utf16, TRY(builtin("net")), JS::default_attributes);
        host->define_direct_property(
            "stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }

    if (name == "dns") {
        host->define_direct_property(
            "native"_utf16, create_dns_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
        auto url = TRY(builtin("url"));
        host->define_direct_property(
            "domainToASCII"_utf16, TRY(url->get("domainToASCII"_utf16)), JS::default_attributes);
    }

    if (name == "https") {
        // Load the authoritative classes first. Only trusted factories receive
        // constructor hooks; no private transport policy is exported publicly.
        TRY(builtin("http"));
        TRY(builtin("tls"));
        host->define_direct_property(
            "httpHooks"_utf16, m_http_client_hooks.ptr(), JS::default_attributes);
        host->define_direct_property(
            "tlsHooks"_utf16, m_tls_client_hooks.ptr(), JS::default_attributes);
        host->define_direct_property("net"_utf16, TRY(builtin("net")), JS::default_attributes);
    }

    if (name == "tls") {
        m_tls_client_hooks = JS::Object::create(m_realm, nullptr);
        host->define_direct_property(
            "clientHooks"_utf16, m_tls_client_hooks.ptr(), JS::default_attributes);
        host->define_direct_property("net"_utf16, TRY(builtin("net")), JS::default_attributes);
        host->define_direct_property(
            "native"_utf16, create_tls_host(m_realm), JS::default_attributes);
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
        auto url = TRY(builtin("url"));
        host->define_direct_property(
            "URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }

    if (name == "net") {
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
        host->define_direct_property("dns"_utf16, TRY(builtin("dns")), JS::default_attributes);
        host->define_direct_property("native"_utf16,
            create_network_host(m_vm, m_realm, m_event_loop), JS::default_attributes);
        host->define_direct_property(
            "stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        auto events = TRY(builtin("events"));
        host->define_direct_property(
            "EventEmitter"_utf16, TRY(events->get("EventEmitter"_utf16)), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }

    if (name == "inspector") {
        host->define_direct_property(
            "native"_utf16, create_inspector_host(m_realm), JS::default_attributes);
    }

    if (name == "util/types")
        host->define_direct_property(
            "native"_utf16, create_util_types_host(m_realm), JS::default_attributes);

    if (name == "console") {
        host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
        host->define_direct_property(
            "types"_utf16, TRY(builtin("util/types")), JS::default_attributes);
        // Preserve the original global logger and Code-OSS's existing pipe wrappers
        // without evaluating a replaced global accessor during builtin construction.
        auto console = m_realm.global_object().get_without_side_effects("console"_utf16);
        host->define_direct_property("console"_utf16,
            console.is_object() ? console : JS::js_undefined(), JS::default_attributes);
    }

    if (name == "util") {
        host->define_direct_property(
            "types"_utf16, TRY(builtin("util/types")), JS::default_attributes);
        host->define_direct_property(
            "native"_utf16, create_encoding_host(m_realm), JS::default_attributes);
        host->define_direct_property(
            "timers"_utf16, TRY(builtin("timers")), JS::default_attributes);
        host->define_direct_property(
            "timerPromises"_utf16, TRY(builtin("timers/promises")), JS::default_attributes);
    }

    if (name == "async_hooks")
        host->define_direct_property("native"_utf16,
            m_event_loop.async_context().create_host(m_realm), JS::default_attributes);

    if (name == "querystring") {
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }

    if (name == "url") {
        host->define_direct_property(
            "native"_utf16, create_url_host(m_realm), JS::default_attributes);
        host->define_direct_property("path"_utf16, TRY(builtin("path")), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }

    if (name == "fs") {
        host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
        host->define_direct_property(
            "stream"_utf16, TRY(builtin("stream")), JS::default_attributes);
        host->define_direct_property("path"_utf16, TRY(builtin("path")), JS::default_attributes);
        auto url = TRY(builtin("url"));
        host->define_direct_property(
            "URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
        host->define_direct_property(
            "fileURLToPath"_utf16, TRY(url->get("fileURLToPath"_utf16)), JS::default_attributes);
        if (!m_filesystem_native)
            m_filesystem_native = create_filesystem_host(m_vm, m_realm, m_event_loop);
        host->define_direct_property(
            "native"_utf16, m_filesystem_native.ptr(), JS::default_attributes);
        auto events = TRY(builtin("events"));
        host->define_direct_property(
            "EventEmitter"_utf16, TRY(events->get("EventEmitter"_utf16)), JS::default_attributes);
        auto buffer = TRY(builtin("buffer"));
        host->define_direct_property(
            "Buffer"_utf16, TRY(buffer->get("Buffer"_utf16)), JS::default_attributes);
    }

    host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
    host->define_direct_property("platform"_utf16,
        JS::PrimitiveString::create(
            m_vm, Utf16String::from_utf8(StringView(native_platform(), strlen(native_platform())))),
        JS::default_attributes);
    host->define_native_function(
        m_realm, "warn"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(vm.argument(0).to_utf16_string(vm));
            auto message = TRY(vm.argument(1).to_utf16_string(vm));
            auto formatted = Utf16String::formatted("{}: {}\n", name, message);
            auto line = MUST(formatted.utf16_view().to_byte_string());
            fwrite(line.characters(), 1, line.length(), stderr);
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    auto value = TRY(JS::call(m_vm, factory, JS::js_undefined(), JS::Value(host.ptr())));
    if (!value.is_object())
        return error("ERR_MITE_BUILTIN_INITIALIZATION", "Builtin factory returned a non-object");
    return value.as_object();
}

JS::ThrowCompletionOr<void> ModuleLoader::register_host_filesystem(std::string const& name)
{
    GC::Root exports { TRY(construct_builtin("fs")) };
    std::vector<std::string> names;
    for (auto exported : fs_exports)
        names.emplace_back(exported);
    return register_host_module(name, *exports, std::move(names));
}

JS::ThrowCompletionOr<void> ModuleLoader::install_globals(
    bool allow_process_signals, bool fork_child)
{
    m_fork_child = fork_child;
    // Portable EventTarget plus authored cancellation state; no LibWeb/Node host.
    auto parsed_globals = JS::Script::parse(
        Utf16String::from_utf8(
            StringView(MITE_WEB_GLOBALS_SOURCE, sizeof(MITE_WEB_GLOBALS_SOURCE) - 1)),
        m_realm, "mite:web-globals"sv);
    VERIFY(!parsed_globals.is_error());
    GC::Root globals_script { parsed_globals.release_value() };
    auto initializer = TRY(m_vm.run(*globals_script));
    VERIFY(initializer.is_function());
    GC::Root globals_factory { initializer.as_function() };
    GC::Root globals_host { JS::Object::create(m_realm, nullptr) };
    globals_host->define_native_function(m_realm, "keepAlive"_utf16,
        GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(m_event_loop)),
            [](auto& captured_loop, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& loop = captured_loop.get();

                loop.keep_weak_target(vm.argument(0));
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    auto globals = TRY(JS::call(m_vm, *globals_factory, JS::js_undefined(), globals_host.ptr()));
    VERIFY(globals.is_object());
    GC::Root installed { globals.as_object() };
    auto hooks = TRY(installed->get("abort"_utf16));
    auto streams = TRY(installed->get("streams"_utf16));
    VERIFY(hooks.is_object() && streams.is_object());
    m_abort_hooks = hooks.as_object();
    m_web_streams = streams.as_object();
    auto stream_state = TRY(installed->get("streamState"_utf16));
    VERIFY(stream_state.is_object());
    m_web_stream_state = stream_state.as_object();
    // Capture original timer functions before application code can replace globals.
    TRY(builtin("timers"));
    auto buffer = TRY(builtin("buffer"));
    auto constructor = TRY(buffer->get("Buffer"_utf16));
    m_realm.global_object().define_direct_property(
        "Buffer"_utf16, constructor, JS::default_attributes);
    auto url = TRY(builtin("url"));
    m_realm.global_object().define_direct_property(
        "URL"_utf16, TRY(url->get("URL"_utf16)), JS::default_attributes);
    m_realm.global_object().define_direct_property(
        "URLSearchParams"_utf16, TRY(url->get("URLSearchParams"_utf16)), JS::default_attributes);
    auto util = TRY(builtin("util"));
    m_realm.global_object().define_direct_property(
        "TextEncoder"_utf16, TRY(util->get("TextEncoder"_utf16)), JS::default_attributes);
    m_realm.global_object().define_direct_property(
        "TextDecoder"_utf16, TRY(util->get("TextDecoder"_utf16)), JS::default_attributes);
    auto perf_hooks = TRY(builtin("perf_hooks"));
    m_realm.global_object().define_direct_property(
        "performance"_utf16, TRY(perf_hooks->get("performance"_utf16)), JS::default_attributes);
    GC::Root native_signals { create_signal_host(m_realm, m_event_loop, allow_process_signals) };
    GC::Root signal_host { JS::Object::create(m_realm, nullptr) };
    signal_host->define_direct_property(
        "exitEvents"_utf16, JS::Value(true), JS::default_attributes);
    signal_host->define_direct_property(
        "childIpc"_utf16, JS::Value(m_fork_child), JS::default_attributes);
    auto events = TRY(builtin("events"));
    signal_host->define_direct_property(
        "EventEmitter"_utf16, TRY(events->get("EventEmitter"_utf16)), JS::default_attributes);
    GC::Root constants { TRY(create_os_constants(m_realm)) };
    signal_host->define_direct_property(
        "signals"_utf16, TRY(constants->get("signals"_utf16)), JS::default_attributes);
    signal_host->define_direct_property(
        "native"_utf16, native_signals.ptr(), JS::default_attributes);
    signal_host->define_native_function(m_realm, "installErrorHandler"_utf16,
        GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(m_event_loop)),
            [](auto& captured_loop, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& loop = captured_loop.get();

                if (!vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid process error dispatcher"_utf16);
                loop.set_process_error_handler(&vm.argument(0).as_function());
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    auto events_source = JS::Script::parse(
        Utf16String::from_utf8(
            StringView(MITE_PROCESS_EVENTS_SOURCE, sizeof(MITE_PROCESS_EVENTS_SOURCE) - 1)),
        m_realm, "mite:process-events"sv);
    if (events_source.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(
            events_source.error().first().to_utf16_string());
    GC::Root events_script { events_source.release_value() };
    auto events_factory = TRY(m_vm.run(*events_script));
    GC::Root events_factory_root { events_factory.as_object() };
    TRY(JS::call(m_vm, events_factory, JS::js_undefined(), signal_host.ptr(), m_process.ptr()));
    // process is cached before stream/fs are resolved: no recursive process construction.
    GC::Root stdio_host { JS::Object::create(m_realm, nullptr) };
    auto stdio_streams = TRY(builtin("stream"));
    stdio_host->define_direct_property(
        "Writable"_utf16, TRY(stdio_streams->get("Writable"_utf16)), JS::default_attributes);
    stdio_host->define_direct_property("fs"_utf16, TRY(builtin("fs")), JS::default_attributes);
    auto stdio_source = JS::Script::parse(
        Utf16String::from_utf8(
            StringView(MITE_PROCESS_STDIO_SOURCE, sizeof(MITE_PROCESS_STDIO_SOURCE) - 1)),
        m_realm, "mite:process-stdio"sv);
    if (stdio_source.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(
            stdio_source.error().first().to_utf16_string());
    GC::Root stdio_script { stdio_source.release_value() };
    auto stdio_factory = TRY(m_vm.run(*stdio_script));
    GC::Root stdio_factory_root { stdio_factory.as_object() };
    TRY(JS::call(m_vm, stdio_factory, JS::js_undefined(), stdio_host.ptr(), m_process.ptr()));
    auto parsed_fetch = JS::Script::parse(
        Utf16String::from_utf8(StringView(MITE_FETCH_SOURCE, sizeof(MITE_FETCH_SOURCE) - 1)),
        m_realm, "mite:fetch"sv);
    VERIFY(!parsed_fetch.is_error());
    GC::Root fetch_script { parsed_fetch.release_value() };
    auto fetch_initializer = TRY(m_vm.run(*fetch_script));
    VERIFY(fetch_initializer.is_function());
    GC::Root fetch_factory { fetch_initializer.as_function() };
    GC::Root fetch_host { JS::Object::create(m_realm, nullptr) };
    fetch_host->define_direct_property(
        "inflate"_utf16, create_inflate_host(m_realm), JS::default_attributes);
    fetch_host->define_direct_property("abort"_utf16, m_abort_hooks.ptr(), JS::default_attributes);
    fetch_host->define_direct_property(
        "webStreams"_utf16, m_web_streams.ptr(), JS::default_attributes);
    fetch_host->define_direct_property(
        "streamState"_utf16, m_web_stream_state.ptr(), JS::default_attributes);
    fetch_host->define_direct_property("Buffer"_utf16, constructor, JS::default_attributes);
    for (auto name : { "URL"_utf16, "URLSearchParams"_utf16 })
        fetch_host->define_direct_property(name, TRY(url->get(name)), JS::default_attributes);
    for (auto name : { "TextEncoder"_utf16, "TextDecoder"_utf16 })
        fetch_host->define_direct_property(name, TRY(util->get(name)), JS::default_attributes);
    auto parsed_decoder = JS::Script::parse(
        Utf16String::from_utf8(
            StringView(MITE_DECOMPRESSION_SOURCE, sizeof(MITE_DECOMPRESSION_SOURCE) - 1)),
        m_realm, "mite:decompression"sv);
    VERIFY(!parsed_decoder.is_error());
    GC::Root decoder_script { parsed_decoder.release_value() };
    auto decoder_initializer = TRY(m_vm.run(*decoder_script));
    VERIFY(decoder_initializer.is_function());
    GC::Root decoder_factory { decoder_initializer.as_function() };
    auto decoder_value
        = TRY(JS::call(m_vm, *decoder_factory, JS::js_undefined(), fetch_host.ptr()));
    VERIFY(decoder_value.is_object());
    GC::Root decoder { decoder_value.as_object() };
    auto decoder_constructor = TRY(decoder->get("DecompressionStream"_utf16));
    m_web_streams->define_direct_property(
        "DecompressionStream"_utf16, decoder_constructor, JS::default_attributes);
    m_realm.global_object().define_direct_property("DecompressionStream"_utf16, decoder_constructor,
        JS::Attribute::Writable | JS::Attribute::Configurable);
    fetch_host->define_direct_property(
        "decompression"_utf16, decoder.ptr(), JS::default_attributes);
    // Transport modules stay lazy: Headers/Response-only programs need no sockets.
    fetch_host->define_native_function(m_realm, "require"_utf16,
        GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();

                auto name = TRY(vm.argument(0).to_utf16_string(vm));
                auto bytes = MUST(name.utf16_view().to_byte_string());
                auto module = string(bytes);
                if (module != "node:http" && module != "node:https" && module != "node:stream"
                    && module != "node:crypto")
                    return self->error(
                        "ERR_MITE_UNSUPPORTED_BUILTIN", "Unexpected fetch transport dependency");
                if (self->m_transport_captured) {
                    auto found = self->m_transport_dependencies.find(module);
                    VERIFY(found != self->m_transport_dependencies.end());
                    return found->second.ptr();
                }

                return TRY(self->builtin(module.substr(5)));
            }),
        1, JS::default_attributes);
    fetch_host->define_direct_property("platform"_utf16,
        JS::PrimitiveString::create(
            m_vm, Utf16String::from_utf8(StringView(native_platform(), strlen(native_platform())))),
        JS::default_attributes);
    auto parsed_data = JS::Script::parse(Utf16String::from_utf8(StringView(MITE_FETCH_DATA_SOURCE,
                                             sizeof(MITE_FETCH_DATA_SOURCE) - 1)),
        m_realm, "mite:fetch-data"sv);
    VERIFY(!parsed_data.is_error());
    GC::Root data_script { parsed_data.release_value() };
    auto data_initializer = TRY(m_vm.run(*data_script));
    VERIFY(data_initializer.is_function());
    GC::Root data_factory { data_initializer.as_function() };
    auto data_value = TRY(JS::call(m_vm, *data_factory, JS::js_undefined(), fetch_host.ptr()));
    VERIFY(data_value.is_object());
    GC::Root data { data_value.as_object() };
    fetch_host->define_direct_property("data"_utf16, data.ptr(), JS::default_attributes);
    for (auto name : { "Blob"_utf16, "File"_utf16, "FormData"_utf16 }) {
        auto value = TRY(data->get(name));
        m_realm.global_object().define_direct_property(
            name, value, JS::Attribute::Writable | JS::Attribute::Configurable);
        if (name != "FormData"_utf16)
            buffer->define_direct_property(name, value, JS::default_attributes);
    }

    auto fetch_globals = TRY(JS::call(m_vm, *fetch_factory, JS::js_undefined(), fetch_host.ptr()));
    VERIFY(fetch_globals.is_object());
    GC::Root fetch_exports { fetch_globals.as_object() };
    // Preserve original networking classes/functions for native embedding services.
    // Public fetch and module exports remain writable without replacing this capability.
    GC::Root transport { JS::Object::create(m_realm, nullptr) };
    auto capture = [&](auto name, JS::Value value) {
        transport->define_direct_property(name, value, JS::Attribute::Enumerable);
    };

    for (auto name : { "Headers"_utf16, "Request"_utf16, "Response"_utf16, "fetch"_utf16 })
        capture(name, TRY(fetch_exports->get(name)));
    for (auto name : { "AbortController"_utf16, "AbortSignal"_utf16 })
        capture(name, TRY(m_realm.global_object().get(name)));
    for (auto name : { "ReadableStream"_utf16, "TransformStream"_utf16 })
        capture(name, TRY(m_web_streams->get(name)));
    for (auto name : { "Readable"_utf16, "Writable"_utf16 })
        capture(name, TRY(stdio_streams->get(name)));
    auto event_exports = TRY(builtin("events"));
    capture("EventEmitter"_utf16, TRY(event_exports->get("EventEmitter"_utf16)));
    capture("Buffer"_utf16, constructor);
    capture("URL"_utf16, TRY(url->get("URL"_utf16)));
    capture("abortHooks"_utf16, m_abort_hooks.ptr());
    m_web_transport = transport;
    for (auto name : { "Headers"_utf16, "Request"_utf16, "Response"_utf16, "fetch"_utf16 })
        m_realm.global_object().define_direct_property(name, TRY(fetch_exports->get(name)),
            JS::Attribute::Writable | JS::Attribute::Configurable);
    GC::Root cryptography { TRY(builtin("crypto")) };
    auto webcrypto = TRY(cryptography->get("webcrypto"_utf16));
    VERIFY(webcrypto.is_object());
    m_web_crypto = webcrypto.as_object();
    GC::Root crypto_getter { JS::NativeFunction::create(m_realm,
        GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();
                return self->m_web_crypto.ptr();
            }),
        0, "get crypto"_utf16) };
    m_realm.global_object().define_direct_accessor("crypto"_utf16, crypto_getter.ptr(), nullptr,
        JS::Attribute::Enumerable | JS::Attribute::Configurable);
    m_realm.global_object().define_direct_property("Crypto"_utf16,
        TRY(webcrypto.as_object().get("constructor"_utf16)),
        JS::Attribute::Writable | JS::Attribute::Configurable);
    if (m_fork_child)
        TRY(builtin("child_process"));

    TRY(install_policy_watcher());
    TRY(install_storage_and_keyboard());
    return {};
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> ModuleLoader::web_transport()
{
    if (!m_web_transport)
        return error("ERR_MITE_GLOBALS_NOT_INITIALIZED", "Transport globals are not initialized");
    if (!m_transport_captured) {
        // Embeddings request this before application entry. Snapshot the small native
        // transport surface instead of consulting mutable public module fields later.
        // Standalone Headers/Response programs retain lazy socket-module construction.
        for (auto const* name : { "http", "https", "crypto" }) {
            GC::Root module { TRY(builtin(name)) };
            GC::Root facade { JS::Object::create(m_realm, nullptr) };
            auto key = std::string_view(name) == "crypto" ? "randomBytes"_utf16 : "request"_utf16;
            facade->define_direct_property(key, TRY(module->get(key)), JS::Attribute::Enumerable);
            m_transport_dependencies.insert_or_assign(std::string("node:") + name, facade);
        }

        GC::Root streams { TRY(builtin("stream")) };
        GC::Root facade { JS::Object::create(m_realm, nullptr) };
        for (auto name : { "Readable"_utf16, "Writable"_utf16 }) {
            auto constructor = TRY(streams->get(name));
            GC::Root owner { TRY(constructor.to_object(m_vm)) };
            GC::Root operations { JS::Object::create(m_realm, nullptr) };
            operations->define_direct_property(
                "toWeb"_utf16, TRY(owner->get("toWeb"_utf16)), JS::Attribute::Enumerable);
            facade->define_direct_property(name, operations.ptr(), JS::Attribute::Enumerable);
        }

        m_transport_dependencies.insert_or_assign("node:stream", facade);
        m_transport_captured = true;
    }

    return GC::Ref<JS::Object> { *m_web_transport };
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
        auto bridge = JS::NativeFunction::create(m_realm,
            GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(*this)),
                [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto* self = &captured_self.get();

                    if (!vm.argument(0).is_string())
                        return self->error(
                            "ERR_INVALID_ARG_TYPE", "Builtin name must be a string", true);
                    auto bytes
                        = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                    return TRY(self->builtin(string(bytes)));
                }),
            1, "getBuiltin"_utf16);
        m_builtin_bridge = JS::SyntheticModule::create_default_export_synthetic_module(
            m_realm, bridge, "mite:internal/builtin")
                               .ptr();
    }

    auto quoted = AK::JsonValue(view(name)).serialized();
    StringBuilder source;
    source.appendff("import get from 'mite:internal/builtin'; const value = get({}); export {{ "
                    "value as default }};",
        quoted);
    auto const* definition = definition_for(name);
    VERIFY(definition);
    size_t index = 0;
    for (auto export_name : definition->exports) {
        StringView exported { export_name.data(), export_name.size() };
        source.appendff(
            "const e{} = value.{}; export {{ e{} as {} }};", index, exported, index, exported);
        ++index;
    }

    GC::Root metadata { m_vm.heap().allocate<ModuleSource>(identity) };
    auto parsed = JS::SourceTextModule::parse(
        Utf16String::from_utf8(source.string_view()), m_realm, view(identity), {}, metadata.ptr());
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    auto module = parsed.release_value();
    m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
    m_builtin_shims.insert(module.ptr());
    return module;
}

}
