# ADR 0006: Node package resolution

Status: implemented subset; behavioral reference Node v24.15.0 on macOS ARM64.

Share package target resolution between ESM and CommonJS, preserving their
separate file resolution rules. ESM uses exact file URLs and mandatory extensions;
CommonJS uses file extensions and directory main/index fallbacks for legacy
packages. Mapped exports/imports targets never receive extension fallback.

Search ascending node_modules directories, including scoped names, and stop at
the first existing package. A blocked export in a nearer package must not cause a
retry in an outer package. Use the nearest package scope for imports and
self-reference; scope lookup stops at a node_modules boundary. Populate module.paths
from those lookup directories, but mutation of module.paths is not supported yet.

Resolve main exports, subpaths, exact keys, and one-star patterns. Exact mappings
win before patterns; patterns prefer the longest prefix, then the longest key.
Conditions run in insertion order and distinguish unmatched from explicitly null
results. Arrays skip invalid targets/unmatched conditions/null alternatives, but
never retry another target because the selected file does not exist. Active
conditions are node, import/require, node-addons and default. module-sync remains
inactive because synchronous require(ESM) is not implemented; user conditions and
global NODE_PATH folders are also unsupported.

Validate relative targets before URL normalization. Reject traversal and
node_modules segments, including percent-encoded variants, and invalid wildcard
captures. Reject encoded separators and null bytes before filesystem access for
both loaders. Numeric condition keys and mixed condition/subpath export maps are
invalid package configurations. These checks enforce package encapsulation, not
an OS filesystem sandbox: Mite application code is privileged main-process code.

Bound recursive targets/aliases to 64 resolver depth steps, returning
ERR_MITE_PACKAGE_RESOLUTION_LIMIT rather than recursing indefinitely. This is an
intentional implementation limit. Built-in names reserved by the current runtime
return an explicit unsupported error, even if a local package has the same name.
They do not silently substitute an npm package for an unimplemented Node API.

Tests construct isolated npm-style trees without installing a package manager.
They compare stdout, exit status, error names and error codes with Node v24.15.0.
They cover scoped packages, lookup precedence, self references, external imports,
condition ordering/nesting, pattern specificity, arrays, blocked paths and invalid
configurations/targets. They also track built-in failures separately as unsupported
behavior, not Node compatibility passes. These fixtures are synthetic; validation
of real npm packages remains a separate required milestone.

Node v24.15.0 ignores non-string/non-container top-level exports fields, while a
numeric value inside a target map is invalid. Its CommonJS loader falls back to
MODULE_NOT_FOUND for # requests without a package imports scope, whereas ESM
uses ERR_PACKAGE_IMPORT_NOT_DEFINED. The implementation follows these observed
behaviors instead of treating every loader edge case identically.

References: [Node packages](https://nodejs.org/api/packages.html),
[Node resolution algorithms](https://nodejs.org/api/esm.html#resolution-and-loading-algorithm),
and the executable reference recorded above. No general compatibility with all
Node versions is inferred from these fixtures.
