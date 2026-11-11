# File permissions, temporary directories and removal

These operations are available through `node:fs` callback/synchronous forms and
`node:fs/promises`. They use real libuv filesystem work. The focused current native
macOS fixture passes; fresh Linux/Windows acceptance of this slice remains pending.

| Operation | Current contract |
| --- | --- |
| chmod(path, mode), chmodSync, promises.chmod | Numeric or octal-string mode applied to the actual path; native errors preserve filesystem error metadata |
| mkdtemp(prefix, options), mkdtempSync, promises.mkdtemp | Secure native temporary-directory creation, six generated suffix bytes; string/default UTF-8 or encoding:'buffer' output |
| rm(path, options), rmSync, promises.rm | Remove a file or final symlink without traversing its target; force:true suppresses ENOENT only |

A directory passed to rm is refused with ERR_FS_EISDIR. Recursive removal and
nonzero retry options fail explicitly with ERR_MITE_UNSUPPORTED_API. They are not
silently ignored. Empty owned temporary directories can use the existing rmdir
operation. mkdtemp preserves the public prefix spelling after native Windows path
conversion; native Windows behavior is not yet accepted for this change.

The maintained mutation fixture creates only owned inputs, changes their mode,
checks secure temporary creation and file/symlink removal, verifies the target
survives symlink removal, and cleans its own directories. The Node reference and
actual LibJS/Mite execution pass. Broader filesystem bounds remain described in
[Mite's runtime reference](mite.md); this is not full Node filesystem compatibility.
