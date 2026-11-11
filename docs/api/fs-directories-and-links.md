# Directory streams, links and recursive copies

Mite provides `link`/`linkSync`, `readlink`/`readlinkSync`,
`symlink`/`symlinkSync`, `utimes`/`utimesSync`, callback `opendir`,
`opendirSync`, and their promise forms. Link and timestamp operations use real
libuv filesystem calls through the existing native argument-snapshot and worker
completion pipeline. `readlink` returns the actual link contents as text or a
Buffer; it does not canonicalize the target. Native errors retain paths and, for
link creation, the destination.

A `Dir` owns a genuine `uv_dir_t`, populated by `uv_fs_opendir`, incrementally
read by `uv_fs_readdir` and released by `uv_fs_closedir` or native RAII cleanup.
`read`/`close` accept callbacks or return promises; sync operations and async
iteration are available. Async operations serialize per stream. Concurrent sync
operations fail explicitly. Limits are 256 native directory leases, 4096 entries
per native batch and 256 waiting operations per stream. Recursive `opendir` is
unsupported. Queued callback contexts follow native queue promotion, including
Node24's behavior where queued reads can inherit the first physical read's context.

`fs.cp` and `fs.promises.cp` copy real files/directories/symlinks. Supported options
are `recursive`, `force`, `errorOnExist`, `dereference`, `preserveTimestamps`,
`verbatimSymlinks`, `mode` and an optionally async `filter`. The traversal uses
streaming directory handles, rejects copying a directory into itself and detects
dereferenced directory cycles. It limits depth to 128 and visits to 100,000.
Special filesystem objects, non-UTF8 recursive byte paths and unrecognized options
fail explicitly. `cpSync` is not implemented. Recursive copying is not atomic;
errors can leave already copied entries. No rollback deletes existing user data.

`mite/tests/filesystem-primitives/main.mjs` is the focused real on-disk reference:
links, timestamps, actual directory reads/iteration/close and recursive binary,
symlink, filter, overwrite and self-copy refusal behavior. Platform-specific
symlink privileges and filesystem behavior still require their own acceptance.
