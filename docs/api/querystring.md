# node:querystring

Mite exposes the maintained Node24-derived query parser/stringifier already pinned
for legacy URL compatibility. `querystring` and `node:querystring` share the builtin;
ESM named/default exports and CommonJS use the same operations.

| API | Current contract |
| --- | --- |
| parse / decode | Parse custom separator/assignment strings into a null-prototype map; duplicates become arrays; maxKeys defaults to 1000, nonpositive values remove that limit |
| stringify / encode | Encode supported primitive values and arrays, with custom separator/assignment and encodeURIComponent option |
| escape | Percent-encode UTF-8 text; invalid lone surrogates throw URIError / ERR_INVALID_URI |
| unescape | Decode percent escapes with forgiving malformed-byte fallback |
| unescapeBuffer | Return decoded raw bytes, optionally interpreting plus as space |

Custom encode/decode callbacks follow the pinned Node behavior. Prototype names
such as __proto__ are ordinary map entries. The parser and legacy URL code share
the same maintained implementation and exact percent codec; source licenses and
hashes remain under vendor/node-url.

Focused Node/reference and actual LibJS/Mite checks pass custom codecs/separators,
duplicate/null-prototype maps, maxKeys, arrays/bigints and malformed bytes/surrogates.
The current native builtin contract passes on macOS; fresh Linux/Windows acceptance
of this new public binding is pending. This does not establish undici compatibility.
