# Mite and Firefly documentation

Mite runs JavaScript on LibJS. Firefly uses Mite for its main process and Ladybird
for desktop rendering. Start with a working example, then check the supported API
subset before migrating an application.

**Current native evidence:** macOS ARM64. Native Windows bring-up is running;
Windows/Linux runtime and desktop releases remain unverified. Public preload,
contextBridge and IPC are not available yet. See [compatibility](compatibility/mite-0.1.md)
and [implementation progress](PROGRESS.md) for the actual limits.

## Start here

- [Build from source and run your first example](getting-started.md)
- [Build a CLI tool](guides/mite-cli.md)
- [Run an HTTP server](guides/http-server.md)
- [Create a Firefly desktop app](guides/first-desktop-app.md)
- [Browse the runnable examples](guides/examples.md)

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
- [app](api/app.md), [BrowserWindow](api/browser-window.md), [webContents](api/web-contents.md)

Some APIs still have summary-level reference coverage. The [documentation baseline](reference/documentation-baseline.md)
tracks the additional detail required; undocumented behavior must not be assumed
compatible with Node or Electron.

## Migrate, diagnose and distribute

- [Moving a Node application to Mite](migration/from-node.md)
- [Moving an Electron application to Firefly](migration/from-electron.md)
- [Security boundaries for application authors](guides/security.md)
- [Testing and debugging](guides/testing-and-debugging.md)
- [Troubleshooting](guides/troubleshooting.md)
- [Packaging and release guide](guides/packaging.md)

## Contribute and follow progress

- [Delivery and documentation roadmap](roadmap.md)
- [Measured npm packages](compatibility/npm-packages-0.1.md)
- [Firefly build and internal test gates](development/renderer-build.md)
- [Native Windows bring-up](development/windows-build.md)
- [Updating unmodified Ladybird](development/updating-ladybird.md)
- [Writing and maintaining documentation](development/documentation.md)
- [Implementation progress](PROGRESS.md)
