# Mite CLI reference

Status: implemented source CLI, tested on macOS ARM64. Windows native bring-up and
Linux validation are incomplete. This describes Mite's flags, not Node's CLI.

## Invocation

```sh
mite script.js [arguments...]
mite script.mjs [arguments...]
mite script.cjs [arguments...]
mite -e 'console.log("hello")'
mite --help
mite --version
```

| Input | Contract |
| --- | --- |
| Entry file | Load according to extension/package type; trailing arguments are exposed through process.argv |
| -e source / --eval source | Evaluate source text as a script; not a CommonJS module wrapper |
| --help | Print usage; supported as the sole argument, exit 0 |
| --version | Print current development version and LibJS identity; supported as the sole argument, exit 0 |
| No arguments | Print usage, exit 1; there is no interactive REPL |
| Missing eval source or unknown leading flag | Report command-line error, exit 9 |

File paths and script arguments are not shell-interpolated by Mite. Your shell still
applies its own quoting rules. Use the executable path from your build; no global
installer or package-manager command is provided.

## Script arguments and modules

For file execution, process.argv contains the executable path, entry path and
following script arguments. process.execPath identifies the executable. See
[process and globals](mite.md) for the supported subset.

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

## Unsupported flags and environment

Node/V8 flags, --inspect, --test, --loader, watch mode, TypeScript transformation,
REPL and single-executable compilation are unavailable. NODE_OPTIONS is not a
supported way to configure Mite. Existing npm tooling can prepare JavaScript but is
not embedded in the runtime.

## Evidence and related material

Implementation: [Runtime.cpp](../../mite/runtime/Runtime.cpp).
Behavioral checks: [runtime runner](../../tooling/test_runtime.py) and its CLI
fixtures. [Build a CLI tool](../guides/mite-cli.md) provides an executable file-hash
recipe. The [support policy](../reference/support-policy.md) explains snapshot status.
