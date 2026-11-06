# 0064: Preserve Windows module-path spelling while resolving links

Status: candidate implemented; local POSIX regression checks pass, native Windows
acceptance remains pending.

## Problem and decision

Windows run 42143633063 loaded the Unicode module fixture but failed its exact
metadata comparison: Node retained the RUNNER~1 temporary-directory alias while
std::filesystem::canonical expanded it to runneradmin. This affects CommonJS
filenames, require.resolve, module search paths and ESM identity/metadata.

Use one owned ModulePaths::real_path boundary for all five loader canonicalization
sites. Keep the existing std::filesystem implementation on POSIX. On Windows,
normalize an absolute path, inspect each component with libuv lstat, and replace
symbolic links/junctions with their readlink targets. Ordinary components retain
their spelling, including short names and case. Follow targets before continuing
and reject errors or excessive link traversal. No native final-path call expands
ordinary aliases, and no lexical-only substitute bypasses link resolution.

The behavior follows the distinction between Node's
[realpathSync implementation](https://github.com/nodejs/node/blob/v24.15.0/lib/fs.js)
and its native variant. The
[ESM resolver](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/modules/esm/resolve.js)
uses the former as well. This is an independently written C++/libuv adapter; Node
is a test reference and is never embedded. Ladybird remains unmodified.

## Validation and limits

Expand the existing native module-path differential suite: compare raw Node/Mite
metadata for Unicode CJS/ESM/npm files, linked directory entries and mixed-case
Windows entry spelling. Use a mandatory directory junction on Windows without
requiring symlink privileges. Imports through physical and linked paths must share
both ESM and CommonJS module instances. Exercise native GetShortPathNameW aliases
when the volume exposes them, reporting absence explicitly.

Do not compare spelling to Python Path.resolve: it expands Windows aliases too.
Keep the exact Node comparison and additionally check samefile for filesystem
identity. No observed output is rewritten or case-folded. Distinct non-link
spellings need not collapse into a single cache key if Node preserves them.
UNC module URLs remain explicitly unsupported. This change does not add public
fs.realpath APIs or preserve-symlinks CLI options, nor certify all reparse-point,
network-volume or Windows namespace variants. Full native Windows validation is
still required; local POSIX success cannot prove the Windows branch.
