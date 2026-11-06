# Renderer module loading acceptance

These are actual file assets loaded by separate Firefly/Ladybird and Electron
renderer processes. The staging runner copies files without bundling or inlining
the browser module graph. A classic script is an explicit control, and the .txt
fixture tests MIME refusal rather than malformed JavaScript.

The common fixture checks static/dynamic imports, live bindings/cache identity,
space/Unicode names, reload, MIME/CSP and absent Node capabilities. Firefly-only
checks retain fetch(file:) and HTTP-to-file module refusals. Native policy tests
also check disabled helper behavior, missing client snapshots, opaque origins,
workers, non-script destinations and non-CORS requests.
