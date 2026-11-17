# Console basics

The global console is completed before embedding hooks or application code run,
after real process stdout/stderr are installed. `console.info` and `console.debug`
are callable without importing `node:console`; a later import returns that same
object and captured methods keep their identity. All global output methods use
the actual stdout/stderr Writable queues, so mixing log/info/debug does not bypass
stream ordering or installed write hooks. Minimal native bootstrap writers are
retired before any application captures them.

`node:console` exposes a real stream-backed Console constructor. New instances
support log/info/debug/warn/error/dir/assert with stdout/stderr Writable sinks,
bound methods and write-error handling. Module construction preserves the existing
global object and authored methods/accessor wrappers, including Code-OSS
parent-process logging hooks; presence checks do not execute those getters.

The global instance retains the runtime's safe diagnostic formatting: arguments
are space-separated, objects/functions use native class labels without author
getters, Proxy traps or coercion hooks, and native Error values retain safe
name/message and real engine stack text. Its private native formatter performs no
I/O and limits argument text to 1 MiB of UTF-8, excluding Console's newline/prefix;
overflow throws `ERR_MITE_CONSOLE_LIMIT`. This is not Node's full global inspector
or placeholder formatter.

Custom `new Console(...)` instances keep the existing smaller JS formatter: simple
primitive placeholders and bounded own-data inspection, depth two, at most 100
object keys and 1 MiB formatted text. Accessors are displayed without executing
their getters; Proxy inspection and ANSI colors are explicitly unsupported.
Advanced table, timer/counter/group/profiling methods reject rather than silently
ignoring calls. No full Console parity is claimed.

Focused Node/reference and actual Mite controls pass stream output, detached
methods, assertions/errors, existing wrappers, pre-import global info/debug,
shared identity and output ordering. The standalone runtime CLI suite, including
unchanged safe-object logging, passes; existing builtin/stdio/PTY controls pass.
The rebuilt SDK and the same Console controls in Firefly-as-Node also pass locally
on macOS. Native boundary controls pass standalone and embedded: exactly 1 MiB
ASCII argument text plus the Console newline reaches the writer hook; multibyte
Unicode and separator overflow reject atomically with `ERR_MITE_CONSOLE_LIMIT`
without invoking the writer. Evidence: `build/global-console-{mite-build,firefly-build,controls,
runtime-controls,existing-controls,embedded-controls}.log`. Fresh Linux/Windows
acceptance remains open. Later controlled Code-OSS editor/terminal runs pass with
zero cleanup signals, including a repeat without diagnostic flags; see the
[current checkpoint](../compatibility/vscode.md#controlled-code-oss-terminal-result).
This does not attribute all terminal/IPC behavior to Console bootstrap.

Subsequent Linux/macOS CI exposed a transport regression in always-asynchronous
filesystem-backed stdio: small exit/fatal messages could be deferred, unrelated
unreferenced work kept alive and output blocked behind occupied filesystem workers.
The [native stdio transport](process.md#standard-output) now removes that dependency
while retaining Console identity, formatting and write hooks. Actual local macOS
exit/immediate/abort/process-error/DNS, Console and builtin controls pass after the
Release rebuild, together with independent slow-pipe, regular-file, EPIPE and
borrowed-descriptor checks. Evidence: build/native-stdio-{controls,regressions,
console-controls,builtin-controls}.log. Fresh hosted platform acceptance remains open.
The latest real Code-OSS editor/terminal repeat passes, but helper cleanup sends
two signals; it does not establish fully natural helper teardown.
