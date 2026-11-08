# Application paths and persistent profiles

Status: partial native main API. userData is the real application directory, and
sessionData selects the actual native Ladybird profile before it is opened. Local
macOS acceptance proves cookies/localStorage survive two launches in that selected
profile. Fresh Linux execution is required; native Windows desktop is unavailable.

```js
import {app} from 'firefly';
import fs from 'node:fs';
import path from 'node:path';
const directory = path.join(app.getPath('appData'), 'MyApplication');
fs.mkdirSync(directory, {recursive: true});
app.setPath('userData', directory);
// By default sessionData follows userData; optionally choose another existing root.
app.setAppLogsPath(path.join(directory, 'logs'));
```

All methods belong to the actual main app singleton and its explicit Electron
ESM/CommonJS aliases. No renderer/preload path API or filesystem permission is
added. getAppPath() returns the actual application's entry directory, not the
working directory, executable folder or an ASAR virtual path.

## app.getPath(name)

Returns an absolute native string. Supported names:

| Name | Current behavior |
| --- | --- |
| home | Native home directory |
| appData | Native per-user application root: Application Support on macOS, XDG_CONFIG_HOME/~/.config on Linux |
| userData | Application files; default appData/Firefly/<identity> |
| sessionData | Actual profile root; follows userData unless explicitly selected |
| logs | Selected log directory; default userData/logs, created on demand |
| temp | Native temporary directory |
| exe | Actual native executable path, independent of argv[0] |
| crashDumps | Existing [local crash-reporting contract](crash-reporter.md); separate from profile selection |

Default identity uses a safe readable label and a 16-hex SHA-256 suffix. If a
bounded adjacent package.json supplies name, that package name determines identity;
otherwise the absolute entry directory does. Package identities stay stable across
relocation; development apps with no package metadata need an explicit override
to retain a profile when moved. This is a deliberate difference from Electron's
app-name-only default. getName/setName/getVersion/isPackaged are not implied.

userData/sessionData/logs getters ensure the selected directory exists. New native
directories use 0700 on supported POSIX systems; existing directories and their
permissions are not changed. Other getters do not create folders. XDG native path
overrides are respected by the underlying standard-path implementation. Symlinks
are followed normally: the public path is cleaned lexically, while upstream profile
identity resolves the actual root. No automatic migration from former temporary
profiles or Chromium storage occurs.

## app.setPath(name, directory)

Returns undefined. userData, sessionData and logs accept existing absolute native
directories. Missing directories, files, relative paths, NUL, nonstrings and invalid
UTF-8 are errors. Other mutable names remain explicitly unsupported; crashDumps
keeps its independent freeze rules. Rejected setters retain the previous selection.

Before readiness, changing userData also changes the default sessionData and log
locations. An explicit sessionData/log override stays independent. Set profile
paths synchronously at bootstrap, after creating the folder with fs.mkdirSync.
userData/sessionData seal when native initialization begins, before queued ready
delivery. Later changes throw ERR_FIREFLY_PATHS_TOO_LATE; a live database/profile
is never rebound by changing a returned string. logs can be changed later.

## app.setAppLogsPath([directory])

Returns undefined. Unlike setPath, this creates the directory and missing parents.
Omitting the argument uses userData/logs on the current platforms, including macOS;
this differs from Electron's macOS Library/Logs convention. getPath('logs') returns
the selected folder. This is a filesystem/log-location API, not a new logging
library or a promise that native stdout/stderr are redirected there.

## Real profile startup and storage layout

Qt and Mite's main bootstrap start first. After main bootstrap path selection,
Firefly seals paths and initializes upstream WebView::Application with the selected
sessionData as --profile-path. Only then are settings, the SQL-backed default
session and native helpers opened and readiness delivered. Early language/theme
choices are held until the selected settings exist. CPU painting remains a launch-
argument policy, consumed before the compositor is started.

The selected native profile owns config/, data/, cache/ and runtime/ subdirectories.
Its SQLite-backed cookie and Web Storage stores are enabled; files and schemas are
Ladybird's, not Chromium's. userData can hold application-owned files independently
when sessionData is selected elsewhere. Unknown cookies/cache/session APIs and
custom partitions do not become supported merely because persistence works.

Initialization failure preserves user files, fails startup and never signals ready.
An early app.exit/error can end bootstrap without opening any profile/helper. Qt
outlives native helper/session/database teardown so normal shutdown can flush
persistent state. Existing application/runtime sandbox and protocol rules remain
in force. A returned main path does not grant a document direct file access.

## Errors and validation

JS argument checks use ERR_INVALID_ARG_TYPE. Native invalid paths use
ERR_INVALID_ARG_VALUE, bounded argument overflow uses ERR_OUT_OF_RANGE, unsupported
names use ERR_FIREFLY_UNSUPPORTED_API, and directory creation failures use
ERR_FIREFLY_PATH_FAILED. Native text is independently limited to 32,768 UTF-16 code
units/UTF-8 bytes without NUL. Logs/profile initialization errors are not converted
into successful getters or ready events.

```sh
cmake --build build/renderer --target check-firefly-app-paths --parallel 3
```

The native probe validates real directories, independent session selection, failed
setters, bounds/lifecycle and native executable lookup without a GUI. Public tests
select distinct app/session roots and verify actual settings/SQLite files, app
files, early language/theme, sandbox activation, and cookies/localStorage across
two real launches. A blocked config directory proves failure without readiness or
file deletion. These are focused acceptance checks, not full browser-storage,
profile-concurrency, locking, migration, IndexedDB or all-platform acceptance.
See [ADR 0133](../architecture/0133-deferred-profile-startup.md).
API baseline: [Electron app paths](https://www.electronjs.org/docs/latest/api/app#appgetpathname).
