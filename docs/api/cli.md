# Mite CLI reference

Status: implemented source CLI, tested on macOS ARM64 and in the scoped
[Linux baseline](../development/linux-build.md). Windows native bring-up remains
incomplete. This describes Mite's flags, not Node's CLI.

## Invocation

```sh
mite script.js [arguments...]
mite script.mjs [arguments...]
mite script.cjs [arguments...]
mite -e 'console.log("hello")'
mite -p '1 + 2'
mite --expose-gc app.mjs
printf '40 + 2' | mite --expose-gc -p
mite --help
mite --version
```

| Input | Contract |
| --- | --- |
| Entry file | Load according to extension/package type; trailing arguments are exposed through process.argv |
| -e source / --eval source | Evaluate source text as a script; not a CommonJS module wrapper |
| -p [source] / --print [source] | Evaluate as a script and print its completion using JavaScript string conversion; without an inline expression/file, read actual piped stdin. Interactive REPL input is rejected. Objects do not receive Node util.inspect formatting |
| --expose-gc | Install the opt-in native synchronous gc function described below; keep the exact flag in process.execArgv |
| -- | End runtime option parsing; later literals are application arguments or the entry filename |
| --help | Print usage and exit 0 |
| --version | Print current development version and LibJS identity, exit 0 |
| No arguments | Print usage, exit 1; there is no interactive REPL |
| Missing eval source or unknown leading flag | Report command-line error, exit 9 |

File paths and script arguments are not shell-interpolated by Mite. Your shell still
applies its own quoting rules. Use the executable path from your build; no global
installer or package-manager command is provided.

## Script arguments and modules

For file execution, process.argv contains the executable path, entry path and
following script arguments. process.execPath identifies the executable. See
[process and globals](mite.md) for the supported subset.

The Windows entry point receives wide CRT arguments through wmain and converts
them to UTF-8 for the runtime, preserving valid Unicode instead of using the ANSI
codepage. Conversion failure, including an unpaired UTF-16 surrogate, reports a
command-line error and exits 9 before JavaScript runs. The Windows behavior suite
at 1df78c8 passes Unicode eval/quoted arguments and malformed-surrogate rejection;
the overall platform suite is still incomplete. Entry filenames now also pass through explicit
UTF-8/native filesystem conversion. Unicode entry/module/package paths have a
local Node differential test; Windows path and interactive-console parity still
require their separate native platform results.

Use .mjs for ESM and .cjs for CommonJS; .js follows package.json type. Eval does not
provide require, module or __dirname as a CommonJS wrapper. Check the
[module guide](../concepts/modules-and-packages.md) for resolution/interoperation limits.

## Completion and exit status

Ordinary successful completion exits 0 after pending referenced work drains.
Read/parse/evaluation errors and fatal unhandled failures normally exit 1. An
unsettled top-level await with no work to settle it exits 13. process.exitCode can
set the final status; process.exit requests immediate termination rather than
waiting for asynchronous work. Native OS exit-code representation can differ.

These are current CLI outcomes, not a promise that every engine/native error
matches Node's message or stack. Catch recoverable failures and close resources;
see [async lifetime](../concepts/async-and-lifetime.md).

## Native garbage collection

`--expose-gc` installs a real `globalThis.gc` on the owning LibJS VM before host
initialization and entry evaluation. Default execution has no `gc` global. The spelling follows the
[Node 24 CLI reference](https://nodejs.org/download/release/v24.15.0/docs/api/cli.html#--expose-gc).
`gc()` and `gc(undefined)` synchronously run the ordinary LibJS heap collector
and return undefined. Live roots and objects kept alive during the current job
remain protected; unreachable targets can be collected. This is not a V8 backend
or an all-objects destruction operation.

The function has name `gc`, length 0 and a writable/enumerable, nonconfigurable
global property, matching the measured Node surface. Option bags, non-undefined
arguments and extra arguments throw `ERR_MITE_UNSUPPORTED_API` without invoking
option getters. Minor/major generation selection, asynchronous collection and
V8-specific flavors are unavailable; collection may block this VM owner thread.

```js
// Run with: mite --expose-gc app.mjs
const kept = { bytes: new Uint8Array([0, 128, 255]) };
gc();
console.log(kept.bytes[2]); // 255: live values remain rooted
```

Options are parsed until the entry filename, the first eval application argument,
or `--`. A GC flag following `-e`/`-p` source is still a runtime flag until that
boundary. `process.execArgv` preserves actual option/source order and duplicates;
flags after the entry/boundary remain in `process.argv`. With optional print and
no inline source/file, piped bytes are read before VM execution and read errors
fail; an empty expression is distinct from omitted source.

The native Windows CLI selects binary stdin/stdout/stderr. Embedded callers of
`Mite::run()` retain control of their CRT stream modes. This new input behavior
has local macOS differential acceptance; fresh native Windows execution remains
pending. Standalone worker/fork engine-flag inheritance is not established by this
slice. Firefly [utilityProcess](utility-process.md) has its own explicit validated
GC opt-in; it does not inherit parent engine flags.

The maintained [GC runner](../../tooling/tests/test_expose_gc.py) passes 30 actual
Node/Mite CLI controls locally, including rooted/weak collection, positional
boundaries, piped/EOF/invalid source, explicit option refusal and native read/TTY
errors. These results do not establish V8 GC-option parity or new platform support.

## Unsupported flags and environment

Apart from the explicit native --expose-gc subset above, Node/V8 flags, --inspect,
--test, --loader, watch mode, TypeScript transformation,
REPL and single-executable compilation are unavailable. NODE_OPTIONS is not a
supported way to configure Mite. Existing npm tooling can prepare JavaScript but is
not embedded in the runtime.

## Evidence and related material

Implementation: [Runtime.cpp](../../mite/runtime/core/Runtime.cpp).
Behavioral checks: [runtime runner](../../tooling/tests/test_runtime.py) and its CLI
fixtures. [Build a CLI tool](../guides/mite-cli.md) provides an executable file-hash
recipe. The [support policy](../reference/support-policy.md) explains snapshot status.
