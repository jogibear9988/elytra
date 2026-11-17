# Data-URL modules

Status: partial native ESM support in standalone Mite and Firefly's embedded Mite,
locally verified on macOS ARM64 against Node 24.15. A controlled original Code-OSS
extension workflow also passes; fresh Windows/Linux acceptance remains pending. This uses LibJS, LibURL
and AK; it adds no Node/V8, Firefly or LibWeb dependency to standalone Mite.

```js
const javascript = 'export const answer = 42;';
const module = await import('data:text/javascript,' + encodeURIComponent(javascript));
console.log(module.answer);

const config = await import('data:application/json,%7B%22enabled%22%3Atrue%7D', {
  with: { type: 'json' }
});
console.log(config.default.enabled);
```

## Source and format

Supported JavaScript MIME essences are text/javascript and application/javascript.
Their ASCII case and surrounding whitespace are normalized, matching the measured
Node format selection. JSON requires the exact application/json essence; changing
its case or adding whitespace to the essence fails with ERR_UNKNOWN_MODULE_FORMAT.
MIME parameters such as `;charset=utf-8` are accepted. These are UTF-8 module
sources, not a general charset-transcoding or network Fetch API.

Percent-encoded bodies and `;base64` bodies are supported. Base64 follows forgiving
decoding: ASCII whitespace and omitted padding are accepted; malformed padding,
invalid characters and the URL-safe alphabet are rejected. Decoded source uses
UTF-8 replacement for malformed sequences. A leading UTF-8 BOM is removed before
JSON parsing; its original bytes still count toward admission.

JSON requires `with: { type: 'json' }`. Missing, unsupported or incompatible import
attributes fail explicitly. JavaScript compiles and executes as a real LibJS ES
module; JSON uses the native JSON module parser and exports its actual value.
A MIME name or successful resolve result does not substitute for source parsing.

## Identity, resolution and hooks

The complete serialized URL is the cache key, including its query and fragment.
The fragment is excluded from source decoding, so two fragments can run separate
module instances from the same body. A query is part of the data body, not a
filesystem search string; percent-encode delimiters when they belong inside source
text. Repeated imports of the same URL reuse its actual native module record.

import.meta.url contains that complete identity. Data modules have no
import.meta.filename or import.meta.dirname. They can import supported builtins,
other absolute data URLs and absolute file URLs through the ordinary resolver.
Relative paths and package lookup have no file base and fail; the loader does not
coerce a data URL into a pathname or silently use the current working directory.
These modules are application code, not a filesystem sandbox: an explicit builtin
or absolute file import retains its ordinary application authority.

Synchronous [registerHooks](module.md#registerhooksoptions) resolve hooks can return
a data URL and matching module/json format hint. Data-parent contexts and JSON
attributes are preserved through nextResolve. General load hooks and format/source
overrides remain unsupported. Data modules never acquire the private CJS, builtin
or host bridge access held by loader-generated shim records. Public hook results
cannot turn their URL text into that private module identity.

A generic Code-OSS-shaped fixture generates ESM that obtains actual API objects
from an application-owned registry. Default/named exports, object identity and real
method calls pass, including forced GC. The controlled Code-OSS workflow below
also exercises the original API resolver without a data-module application patch;
this does not establish general extension or editor acceptance.

## Limits and failures

Limits are per ModuleLoader instance and are deliberate Mite differences:

| Limit | Bound |
| --- | --- |
| Serialized data URL | 8 MiB |
| Decoded source | 4 MiB |
| Successful cached data-module records | 1,024 |
| Aggregate retained byte charge | 32 MiB, charging twice the URL bytes plus twice the decoded source bytes |

Cache hits and failed parses do not consume another reservation. Successful records
remain rooted for the loader's lifetime, alongside its normal ESM cache. The byte
charge conservatively accounts for source and identity storage; it is not a hard
maximum for ASTs, exported application objects, transient decoder allocations or
the entire VM heap. The independent record bound limits tiny-module metadata.

| Failure | Result |
| --- | --- |
| Missing comma or malformed base64 body | TypeError, ERR_INVALID_URL |
| Unknown MIME, including non-exact JSON/Wasm essence | ERR_UNKNOWN_MODULE_FORMAT |
| Exact application/wasm MIME | ERR_MITE_UNSUPPORTED_API; no Wasm data-module implementation |
| JSON missing type attribute | TypeError, ERR_IMPORT_ATTRIBUTE_MISSING |
| JSON attribute on JavaScript | TypeError, ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE |
| Unsupported attribute | TypeError, ERR_IMPORT_ATTRIBUTE_UNSUPPORTED |
| URL/source/cache/record quota | ERR_MITE_DATA_MODULE_LIMIT |
| Relative/package import without a file parent | TypeError, ERR_UNSUPPORTED_RESOLVE_REQUEST |
| CommonJS require of data modules, directly or via a resolve hook | ERR_MITE_UNSUPPORTED_REQUIRE_ESM |
| Private shim or unregistered host target | ERR_UNSUPPORTED_ESM_URL_SCHEME |

Source/JSON syntax errors remain actual parser errors. CommonJS synchronous ESM
loading, data-URL CLI entry files and Wasm are not added by this import support.

## Validation

`tooling/tests/test_data_modules.py --mite <mite>` compares seven shared groups
with Node, then repeats them with real `--expose-gc` collection. Mite-only controls
cover private bridges, explicit unsupported paths, URL/source and aggregate cache
quotas, recovery and a separate 1,024-record boundary. All pass locally under the
native macOS Mite build; the rebuilt Firefly embedded-runtime runner also passes
all data-module groups, including GC and quotas. Existing standalone module,
resolve-hook and CJS suites also pass.

The original controlled Code-OSS extension fixture passes activation, a registered
command, openTextDocument/showTextDocument and WorkspaceEdit/save. Independent
Python readback verifies the actual 85-byte file. The report records accepted:true,
native exit 0 and zero cleanup signals; all 12 tracked native PIDs are absent after
completion. No new Code-OSS data-module helper or application-source workaround
was added for this result.

This is local macOS component and controlled-workflow acceptance. It does not
verify visible editor rendering, PTYs, arbitrary extensions, synchronous
require(ESM), general native-addon compatibility, or all platforms. Fresh platform results must be
recorded separately.

See [ADR 0202](../architecture/0202-data-url-modules.md),
[resolve hooks](module.md) and the [CLI GC contract](cli.md).
