# Mite and Firefly documentation

Mite runs JavaScript on LibJS. Firefly uses Mite for its main process and Ladybird
for desktop rendering. Start with a working example, then check the supported API
subset before migrating an application.

Start with [current support and tested examples](status.md) for platform evidence,
which Electron examples run, API coverage and the distinction between published
releases and newer master changes.

**Current native evidence:** macOS ARM64, plus the scoped
[Linux](development/linux-build.md) and [Windows](development/windows-build.md)
x64 Mite runtime gates. Windows/Linux desktop releases remain unfinished.
[CommonJS preload/contextBridge](api/context-bridge.md) has partial native macOS
support, including bounded [invoke/handle IPC](api/ipc.md). See [compatibility](compatibility/mite-0.1.md)
and [implementation progress](PROGRESS.md) for the actual limits.

[macOS ARM64 Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2)
ships the verified `19ad113` snapshot, including public preload/contextBridge,
request/reply IPC and modern ESM examples. Current master can contain later
changes; check the release's pinned source/metadata before assuming its binaries
include them. Linux has scoped experimental X11 application acceptance; Linux
and Windows desktop packages remain unfinished.

## Start here

- [Build from source and run your first example](getting-started.md)
- [Build a CLI tool](guides/mite-cli.md)
- [Run an HTTP server](guides/http-server.md)
- [Create a Firefly desktop app](guides/first-desktop-app.md)
- [Browse the runnable examples](guides/examples.md)
- [Desktop feature examples: all Electron guide topics](examples/index.md)

## Understand the model

- [Runtime and process boundaries](concepts/runtime-and-processes.md)
- [Modules and packages](concepts/modules-and-packages.md)
- [Asynchronous work and resource lifetime](concepts/async-and-lifetime.md)
- [Compatibility and support labels](reference/support-policy.md)

## Find a contract

- [API overview](api/index.md)
- [Mite runtime reference](api/mite.md)
- [Firefly desktop reference](api/firefly.md)
- [CLI reference](api/cli.md)
- [Renderer ES modules](guides/renderer-modules.md)
- [app](api/app.md), [BrowserWindow](api/browser-window.md), [webContents](api/web-contents.md)

Some APIs still have summary-level reference coverage. The [documentation baseline](reference/documentation-baseline.md)
tracks the additional detail required; undocumented behavior must not be assumed
compatible with Node or Electron.

## Migrate, diagnose and distribute

- [Moving a Node application to Mite](migration/from-node.md)
- [Moving an Electron application to Firefly](migration/from-electron.md)
- [VS Code / Code-OSS target and confirmed blockers](compatibility/vscode.md)
- [Files and binary data across processes](guides/file-transfer.md)
- [Security boundaries for application authors](guides/security.md)
- [Testing and debugging](guides/testing-and-debugging.md)
- [Troubleshooting](guides/troubleshooting.md)
- [Packaging and release guide](guides/packaging.md)

## Contribute and follow progress

- [Delivery and documentation roadmap](roadmap.md)
- [Measured npm packages](compatibility/npm-packages-0.1.md)
- [Firefly build and internal test gates](development/renderer-build.md)
- [Native Windows bring-up](development/windows-build.md)
- [Native Linux bring-up](development/linux-build.md)
- [Updating unmodified Ladybird](development/updating-ladybird.md)
- [Writing and maintaining documentation](development/documentation.md)
- [Implementation progress](PROGRESS.md)
