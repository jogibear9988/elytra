# app.commandLine

Status: partial native main-process command line. Reads inspect a snapshot of real
application launch switches. Supported engine changes have actual native effects;
unknown Chromium/Blink/V8 switches are explicitly unsupported when appended/removed.
This is not a general Chromium switch implementation.

```js
import {app} from 'firefly';
app.commandLine.appendSwitch('lang', 'de-DE');
console.log(app.commandLine.getSwitchValue('lang'));
// Create windows after app.whenReady(); their preferred Web language is de-DE.
```

The readonly app.commandLine singleton is shared by Firefly and the explicit
Electron ESM/CommonJS main aliases. It is not a separately exported CommandLine
constructor. Renderer/preload and standalone Mite do not expose it. No method
changes process.argv or enables the executable-owned Electron import option.

## Observation

| Method | Returns |
| --- | --- |
| hasSwitch(name) | boolean; true for an explicitly present switch, even without a value |
| getSwitchValue(name) | string; empty if absent or present without a value |

Initial scanning starts after the executable/application entry and after native
host-option stripping. The executable-owned --electron-compat is excluded; the same
token passed after the entry remains an ordinary application argument. Parse -name,
--name and --name=value. Split at the first '='; later duplicates win. Positional
arguments do not supply switch values or stop scanning. A standalone '--' stops
switch scanning; later tokens remain untouched in process.argv.

Reading an opaque launch switch only observes its token; it does not imply engine
support. Use process.argv for application-specific command-line parsing. Initial
noncanonical names remain opaque application arguments and are excluded from this
snapshot. The native parser does not copy the synthetic Ladybird browser arguments
or invent Chromium defaults. Mutations update this separate native snapshot only.
Reads remain available after readiness.

Names are lowercase ASCII letters/digits/hyphens, starting with a letter/digit,
1–128 bytes, without leading dashes. This is a deliberate validation subset.
Invalid names use ERR_INVALID_ARG_VALUE, nonstrings ERR_INVALID_ARG_TYPE.

## Engine changes

| Method | Current supported behavior |
| --- | --- |
| appendSwitch('lang', value) | Before readiness, set the native website preferred language list to one canonical supported Unicode locale tag |
| removeSwitch('lang') | Before readiness, restore the native language list captured before application switch application |
| appendSwitch(otherName, [value]) | Reject unsupported engine switches; CPU painting is already frozen at native startup |
| removeSwitch(otherName) | Reject unsupported engine switches; CPU painting is already frozen at native startup |
| appendArgument(value) | Explicitly unsupported |

Successful append/remove return undefined. lang requires a nonempty available
locale of at most 128 UTF-8 bytes. Validation uses the pinned Unicode locale
canonicalizer and availability table. It changes native WebView settings and is
propagated through the existing page/worker language settings machinery. It does
not change the OS locale, translate the application's own labels or claim Electron
getLocale/getPreferredSystemLanguages support. Invalid changes leave the previous
snapshot/effective language state in place.

Select CPU painting at launch:

```sh
build/renderer/bin/firefly main.mjs --force-cpu-painting
```

This valueless Firefly/Ladybird switch is parsed before native Application::create
and passed to the real compositor at its launch. It forces the existing CPU painting
mode; it is not a general disable-all-GPU/video/WebGL promise. appendSwitch or
removeSwitch cannot change it after the application script begins and throw
ERR_FIREFLY_COMMAND_LINE_TOO_LATE. A nonempty launch value fails before the native
application is created. lang can also be supplied initially as --lang=de-DE.
Supported initial values are validated/applied before the main entry runs.

All engine mutations seal when native readiness is scheduled; later mutations
throw ERR_FIREFLY_COMMAND_LINE_TOO_LATE. The compositor's earlier boundary is
intentional: assigning a late option would not reconfigure that running process.
Unknown engine switch mutations use ERR_FIREFLY_UNSUPPORTED_API. In particular,
enable-features, disable-features, disable-blink-features, js-flags, no-sandbox,
remote-debugging-port, proxies and Chromium logging switches are not implemented.
No sandbox-disabling fallback is performed.

## Bounds and native failures

The canonical initial snapshot is limited to 256 unique switches, 32,768 UTF-8
bytes per value and 64 KiB total names/values. Overflow fails startup explicitly.
Positional arguments are not copied into this snapshot and are not subject to
these switch limits. Embedded NUL is rejected; invalid initial UTF-8 values fail
when read. Appended values require strings, with independent JS/native bounds.
Native errors retain codes; snapshot range errors use ERR_OUT_OF_RANGE and invalid
native values use ERR_INVALID_ARG_VALUE. A rejected engine change restores the
prior switch map. These are stricter limits than general Electron command lines.

## Validation and Code-OSS

```sh
cmake --build build/renderer --target check-firefly-command-line --parallel 3
```

The gate checks native parsing/rollback/bounds/sealing and Node/Mite JS policy. Its
public test uses one hidden window, checks actual navigator.languages, inspects only
owned WebContent/compositor PIDs and verifies the compositor's CPU-painting argument
with sandboxing retained. The same native read fixture can additionally be compared
with actual Electron using test_command_line.py --electron <binary>. Exact platform
acceptance is recorded in [PROGRESS](../PROGRESS.md).

Code-OSS reads now resolve, but its next Chromium feature append remains explicitly
unsupported. That engine-specific setup needs a justified audited application
adaptation or a real equivalent, not an inert accepted switch. The workbench still
does not run. See [ADR 0130](../architecture/0130-native-command-line.md).
The API baseline is [Electron CommandLine](https://www.electronjs.org/docs/latest/api/command-line).
