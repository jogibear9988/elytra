# crashReporter

Status: partial main-only local native diagnostics. Local macOS Release tests capture
real Firefly main and owned WebContent crashes, recover the main report on the next
launch, and validate metadata/privacy/file modes. Fresh Linux execution is required;
Windows capture is unavailable. This is not Crashpad/minidump/upload compatibility.

## Start local collection

```js
import {app, crashReporter} from 'firefly';
import fs from 'node:fs';
import path from 'node:path';

const reports = path.join(import.meta.dirname, 'reports');
fs.mkdirSync(reports, {recursive: true});
app.setPath('crashDumps', reports);
crashReporter.start({
  uploadToServer: false,
  productName: 'My App',
  extra: {stage: 'startup'},
  globalExtra: {appVersion: '1.0.0'}
});
```

The explicit Electron alias and CommonJS expose the same main singleton. Renderer,
preload and standalone-Mite crashReporter exports are unavailable. No new crash
library is bundled: Firefly reuses Ladybird's LibCore signal capture and LibWebView
private report storage. Reports are bounded native diagnostic text, not memory
dumps or JavaScript stacks. No report is uploaded by this API.

## crashReporter.start(options)

Returns undefined. Call early in application startup. Main capture starts here and
cannot be disabled afterward. Ladybird helpers already have upstream native capture;
start redirects subsequently saved reports to the configured directory. Global
annotations are also attached by the surviving parent for helpers started before
configuration. Capturing standalone Mite/child_process children is not implemented.

| Option | Current behavior |
| --- | --- |
| uploadToServer | Must explicitly be false; true/default fails with ERR_FIREFLY_UNSUPPORTED_API |
| productName | String, defaults to Firefly; recorded as application metadata |
| companyName | Optional string; recorded as application metadata |
| extra | Initial frozen main-only string annotations |
| globalExtra | Frozen annotations for main/helpers; override duplicate extra keys in the saved main report |
| ignoreSystemCrashHandler | false/default preserves OS crash handling; true explicitly unsupported |
| submitURL | Optional string; unused/unretained while uploads are disabled |
| compress, rateLimit | Optional booleans; upload-only settings are inactive because uploads cannot be enabled |

Unknown options fail explicitly. Names are limited to 256 UTF-8 bytes. Each annotation
group has at most 64 keys; keys use ASCII letters/digits/underscore/period/hyphen and
are 1–39 bytes. Values are strings limited to 2,048 UTF-8 bytes. Native configuration
text is at most 16 KiB and the encoded application metadata at most 8 KiB, including
Unicode escaping. These are strict errors, not Electron's longer-value truncation.

Validate options before calling the native backend. Invalid options do not consume
start. Native parsing independently validates the exact bounded schema. Successful
initialization freezes process-lifetime path/metadata; input mutation cannot change
it. A repeated start throws ERR_FIREFLY_CRASH_REPORTER_STARTED. If native signal/store
initialization itself fails, require a process restart: native signal state cannot
safely be rolled back by resetting a JavaScript object. The failure is reported as
ERR_FIREFLY_CRASH_REPORTER_START_FAILED, never a successful collector.

## crashReporter.getParameters()

After start, returns a fresh Record<string,string> of the initial main-only extra
annotations. globalExtra is excluded; mutating the result does not change reports.
Dynamic addExtraParameter/removeExtraParameter are not implemented yet.

## crashReporter.getUploadToServer()

After start, returns false. Before start, this and getParameters throw
ERR_FIREFLY_NOT_READY. Upload support is not inferred from a configured submitURL.

## crashReporter.setUploadToServer(value)

Boolean false returns undefined and keeps uploads disabled, including before start.
Boolean true throws ERR_FIREFLY_UNSUPPORTED_API; other types use ERR_INVALID_ARG_TYPE.
It cannot stop local capture after successful start.

## app.getPath('crashDumps') / app.setPath('crashDumps', directory)

getPath returns the configured path, defaulting to the user-data base directory's
Firefly/CrashReports folder. The default is shared by Firefly apps; configure an
app-specific folder when needed. It is created/secured on start. setPath accepts an
existing absolute native directory, at most 32,768 UTF-8 bytes without NUL, before
native initialization. It returns undefined. Invalid paths or changing the path
after initialization throw ERR_INVALID_ARG_VALUE. These methods do not require app
readiness. Other app path names remain unsupported.

The report directory is private (0700 on supported POSIX systems), saved text is
read-only (0400), and raw main records remain locked while their owner lives. A clean
exit removes its raw pending record. After a fatal main signal, the next start in
that same directory recovers the original report, retaining the crashed launch's
build/application metadata. Unicode annotations use JSON escapes to satisfy the
upstream raw-record ASCII validation. The system crash handler is still invoked.

Backend diagnostics do not automatically capture URLs, page contents, command lines,
environment variables or general process memory. Application-supplied annotations
are deliberately recorded. Native stack capture can be partial; keep matching
binaries/debug symbols. Helper collection still needs the main process to survive
long enough to format/save its report. Existing upstream crash-review UI is not
accepted as part of this public API subset.

## Remaining support and validation

Uploads/compression transport, report-list APIs (getUploadedReports/getLastCrashReport),
dynamic annotations, renderer annotation APIs, Crashpad payloads and Windows capture
remain unimplemented. No empty report-list placeholder is returned. process.crash
and process.kill are not provided by this feature; the test parent injects a real
signal only into its own checked process tree.

```sh
cmake --build build/renderer --target check-firefly-crash-reporter --parallel 3
```

The gate runs ESM option/snapshot policy and actual public main/owned-helper crashes.
Main capture/recovery needs no window; helper acceptance uses one hidden window.
It validates native PID ancestry, real SIGABRT reports, original Unicode metadata,
main/global separation, private modes, absence of an environment sentinel/paths and
clean-exit cleanup. Code-OSS now resolves this export but stops at missing Menu;
its ignoreSystemCrashHandler request and other services still need work.
See [ADR 0127](../architecture/0127-local-crash-reporting.md).
Baseline: [Electron crashReporter](https://www.electronjs.org/docs/latest/api/crash-reporter).
