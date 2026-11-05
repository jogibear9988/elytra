# Testing and debugging

Use the actual engine/executable under investigation. Passing a browser, Node or
Electron reference alone does not verify Mite/Firefly. The current native host is
macOS ARM64; Windows bring-up is tracked separately.

## Runtime tests

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
cmake --build --preset dev --parallel 4
ctest --preset dev --output-on-failure
```

The enabled suites compare runtime/module/npm behavior, encodings, files, timers,
streams, network/HTTP, OS and crypto, plus dependency/upstream integrity. Individual
Python runners accept --mite to test a particular build. Source checkout rules have
an independent guard: `python3 tooling/check_checkout_paths.py`.

## Native desktop and renderer gates

```sh
python3 tooling/bootstrap_electron.py
cmake --build build/renderer --target check-firefly --parallel 4
cmake --build build/renderer --target check-firefly-renderer --parallel 4
```

The Electron reference is a development/test dependency. Other gates cover native
windows, internal isolation and the separate prototype renderer helper. Run only
after configuring the intended target with the [renderer guide](../development/renderer-build.md).
The visible desktop gate needs an unlocked session; compositor/DOM checks alone do
not prove native pixel presentation.

## Application tests

Use port 0, wait for actual readiness, handle failures and close resources. Compare
small supported programs with the pinned Node/Electron reference where appropriate.
Keep fixtures bounded and distinguish an expected unsupported error from successful
functionality. The Designer example has an explicit --smoke workflow described in
its README; this is not a public IPC or automation API for arbitrary applications.

## Current debugging tools

console.log/error and native test logs are the basic supported route. Reduce a
failure to a small module graph or single native operation, capture stdout/stderr,
exit status and exact input, then compare the reference. Error stacks/inspection are
not identical to Node/Electron.

Node --inspect, V8 inspector/profiling flags, Electron openDevTools and executeJavaScript
are not currently supported application debugging APIs. Use a native debugger for
host/engine faults with matching build symbols; do not describe reference-engine
DevTools as Firefly's debugger. A full user-facing debugger/profiler is a roadmap item.

## Useful evidence

Include source commit, dependency pins, OS/architecture, compiler/build mode, command,
minimal input, output/error code and reproduction rate. CTest failures appear under
Testing/Temporary/LastTest.log in the relevant build directory. Windows Actions retain
native diagnostics; [Windows build notes](../development/windows-build.md) explain
scope. For failures caused by missing options/modules, consult [troubleshooting](troubleshooting.md).
