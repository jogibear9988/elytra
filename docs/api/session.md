# session

Status: default-session protocol subset only. Import from firefly, or the explicit
Electron alias. ESM/CommonJS share the same main-process object.

## session.defaultSession

Available after app readiness; earlier access throws ERR_FIREFLY_NOT_READY. Returns
the existing default desktop browsing-session facade, whose protocol property is
the same singleton as the top-level protocol export.

```js
import {app, protocol, session} from 'firefly';
await app.whenReady();
console.log(session.defaultSession.protocol === protocol); // true
```

The [protocol reference](protocol.md) describes registration, modern Response handlers, asynchronous file
callbacks, file interception, ownership, limits, errors and tested scope. The facade is frozen.

fromPartition, fromPath, custom BrowserWindow partitions/sessions, cookies,
webRequest, permission setters, download APIs and wider session methods are
unimplemented. Unsupported webPreferences remain explicitly rejected; a requested
custom partition is never silently routed through the default session.

Actual public default-session file loading passes the local macOS Release hidden
window gate. This does not establish storage, partition isolation or Windows support.
