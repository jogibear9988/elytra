# ADR 0130: Native command-line observation and effect-specific freeze boundaries

Status: accepted for partial main APIs.

## Context

The actual prepared Code-OSS entry passes Menu/contentTracing imports and reaches
app.commandLine.getSwitchValue. Its immediate next mutations configure Chromium,
Blink and V8 features that do not exist in LibWeb/LibJS. A generic string map that
pretends these features were applied would hide incompatibility.

## Decision

Own a bounded native snapshot from actual StartupOptions-sanitized application
arguments. Keep it separate from process.argv and the synthetic browser arguments.
Provide main-only hasSwitch/getSwitchValue, with native prefix/equals/terminator
parsing. Reject unsupported mutations instead of accepting inert engine settings.
Separate the Qt/LibWeb-independent State/parser, LibJS bindings, owned EnginePolicy
adapter and maintained ESM argument validation.

Implement lang through the existing public WebView settings/language propagation
API. Validate/canonicalize first, then apply; failed mutations restore the snapshot.
Restore the pre-application native language list on removal. Seal mutations when
readiness is scheduled. No change to Ladybird sources or standalone Mite is needed.

The actual compositor starts during Application::create, before the app script.
Therefore force-cpu-painting must be selected in real launch arguments before native
creation and forwarded into that compositor's actual options. Reject later append/
remove even before ready. Do not claim a late WebContentOptions assignment changed
an already-running compositor, or equate CPU painting with disabling every GPU use.
Leave unsupported Chromium/Blink/V8/sandbox options explicit errors.

## Evidence and consequences

Native tests cover actual argument parsing, bounded snapshots, transactional apply
refusal and sealing. JS policy tests run on Node/Mite. A public test creates one
hidden window to inspect real navigator.languages, verifies PID ancestry and checks
only the owned compositor's actual CPU-painting launch argument and unchanged
sandbox policy. A shared read fixture compares native Firefly observations with
actual Electron. This does not prove every Electron switch, locale or platform.
Fresh platform execution and the actual next Code-OSS error are recorded in PROGRESS.

The current stricter native bounds/name validation and supported switch effects are
part of the [public contract](../api/command-line.md). Code-OSS's Chromium-specific
feature configuration remains a separate engine-adaptation problem; advancing an
import/early read is not desktop workbench acceptance.
