# Console basics

`node:console` exposes a real stream-backed Console constructor. New instances
support log/info/debug/warn/error/dir/assert with stdout/stderr Writable sinks,
bound methods and write-error handling. Default/module construction preserves the
existing global console object and its methods/accessor wrappers, including
Code-OSS parent-process logging hooks; it attaches Console and fills missing names.

Formatting is intentionally smaller than Node's full inspector: simple primitive
placeholders and bounded own-data inspection, depth two, at most 100 object keys
and 1 MiB formatted text. Accessors are displayed without executing their getters;
Proxy inspection and ANSI colors are explicitly unsupported. New instances refuse
table, timers/counters/groups/profiling and other advanced methods rather than
silently ignoring them. Existing global logger methods retain their prior behavior.

Focused Node/reference and actual Mite fixtures pass real stream output, detached
methods, assertion/error handling and preserved existing wrappers. Advanced console
parity and fresh Linux/Windows acceptance remain open.
