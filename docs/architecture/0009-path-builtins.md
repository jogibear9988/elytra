# ADR 0009: lexical path builtins outside the engine

Status: implemented subset, tested on macOS ARM64 against Node v24.15.0.

Implement node:path in our embedded JavaScript layer, without engine changes or
filesystem access. POSIX and Windows objects share validation and component
helpers, while preserving their different root, drive, UNC and separator rules.
The default uses the native platform adapter, not the mutable process.platform
property. Resolve reads the retained process object's current cwd/environment on
each call. POSIX cwd conversion on a native Windows host is implemented but still
requires native validation.

CommonJS path, path/posix and path/win32 and their node: ESM aliases reuse the
same objects through the builtin registry. Named exports have the registry's
existing snapshot limitation. No Node source is embedded or invoked at runtime.
The independent implementation is checked against the Node executable reference,
including observable basename suffix edge cases and relative Windows roots.

The differential suite compares 3,968 lexical calls across normalize, resolve,
join, relative, parse/format, dirname, basename, extname, isAbsolute and
namespaced-path conversion. It includes drive-relative and UNC paths, Unicode,
dot segments, invalid argument error names/codes, and colon prefixes exposed by
normalization. An ESM identity/live-cwd scenario also passes. Error messages and
stack formatting are not claimed identical. This corpus is not exhaustive.

matchesGlob is absent. Native Windows drive environment/cwd behavior and Linux
execution remain unverified. Passing path.win32 string operations on macOS does
not establish native Windows runtime compatibility. All code remains outside the
unmodified Ladybird submodule.

Reference: [Node path API](https://nodejs.org/api/path.html).
