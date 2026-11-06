# ES modules in renderer pages

Status: validated native macOS ARM64 subset. Firefly's renderer uses Ladybird's
browser ESM loader. Mite's Node-style main-process module loader is a separate
system. Neither loads Node packages or native builtins into a renderer page.
See Electron's [ESM process distinctions](https://www.electronjs.org/docs/latest/tutorial/esm)
for the migration baseline.

## Load modules from application files

Load your page with `window.loadFile('index.html')` and reference a normal browser
module:

```html
<script type="module" src="renderer.mjs"></script>
```

```js
// renderer.mjs
import { formatMessage } from './message.mjs';
document.querySelector('#message').textContent = formatMessage('Hello');
const feature = await import('./feature.mjs');
feature.start();
```

Both static and dynamic local imports use the browser module map. Imports within
one document share module identity and live bindings; navigation creates a new
document/module map. Relative paths and space/Unicode filenames are tested. CSP
and JavaScript MIME requirements still apply. HTTP-served modules also work in the
Designer sample's tested subset. This is not a claim of complete browser module,
worker, import-map or structured module-type compatibility.

Bare npm specifiers, `node:fs`, `firefly` and other native modules are unavailable in
page code. Bundle browser-compatible npm dependencies for the browser, as the
Designer build does. For privileged work, publish a fixed preload action through
[contextBridge](../api/context-bridge.md) and [invoke/handle IPC](../api/ipc.md).

## Preload ESM is an authoring format

The secure sandbox currently executes the compiled CommonJS preload payload.
Author it as ESM and run the pinned [preload bundler](../api/context-bridge.md),
then point webPreferences.preload at that absolute generated file. External page
ESM support does not change preload execution or give page modules Node access.

## File and network boundaries

The production helper has a narrow integration policy allowing file-origin Window
script modules through Ladybird's existing file subresource loader. It does not
disable CORS generally. HTTP(S) pages cannot import local file modules through this
exception; programmatic `fetch(file:…)` remains blocked. Local workers and
non-script module destinations are outside this policy. The underlying file loader
and sandbox remain in force; this is not an application-root filesystem sandbox.

The [native loading fixtures](../../firefly/tests/loading/README.md) compare actual
Firefly/Electron behavior, including strict MIME and CSP refusal. Native policy
checks cover disabled ordinary helpers and rejected request/client combinations.
Implementation and update guards are recorded in [ADR 0073](../architecture/0073-local-renderer-modules.md).
