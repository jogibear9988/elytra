/**
 * Recognizes reserved Node builtin names, including unsupported modules. Reserving names
 * prevents npm packages from silently impersonating an unavailable native API.
 */

#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace Mite {
// Known names from the Node v24.15.0 behavioral reference, not implemented APIs.
inline constexpr std::string_view builtin_names[] = {
    "_http_agent",
    "_http_client",
    "_http_common",
    "_http_incoming",
    "_http_outgoing",
    "_http_server",
    "_stream_duplex",
    "_stream_passthrough",
    "_stream_readable",
    "_stream_transform",
    "_stream_wrap",
    "_stream_writable",
    "_tls_common",
    "_tls_wrap",
    "assert",
    "assert/strict",
    "async_hooks",
    "buffer",
    "child_process",
    "cluster",
    "console",
    "constants",
    "crypto",
    "dgram",
    "diagnostics_channel",
    "dns",
    "dns/promises",
    "domain",
    "events",
    "fs",
    "fs/promises",
    "http",
    "http2",
    "https",
    "inspector",
    "inspector/promises",
    "module",
    "net",
    "os",
    "path",
    "path/posix",
    "path/win32",
    "perf_hooks",
    "process",
    "punycode",
    "querystring",
    "readline",
    "readline/promises",
    "repl",
    "stream",
    "stream/consumers",
    "stream/promises",
    "stream/web",
    "string_decoder",
    "sys",
    "timers",
    "timers/promises",
    "tls",
    "trace_events",
    "tty",
    "url",
    "util",
    "util/types",
    "v8",
    "vm",
    "wasi",
    "worker_threads",
    "zlib",
    "node:sea",
    "node:sqlite",
    "node:test",
    "node:test/reporters",
};

inline bool bare_builtin(std::string_view name)
{
    return std::find(std::begin(builtin_names), std::end(builtin_names), name)
        != std::end(builtin_names);
}

inline bool known_builtin(std::string const& name)
{
    return bare_builtin(name) || bare_builtin("node:" + name);
}
}
