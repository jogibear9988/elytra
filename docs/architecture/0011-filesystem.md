# ADR 0011: native filesystem work with VM-owned completions

Status: implemented subset, tested on macOS ARM64 against Node v24.15.0.

Use libuv filesystem operations from our C++ adapter. Synchronous APIs execute
those calls directly. Callback and Promise APIs queue whole operations on libuv's
worker pool through EventLoop. Each worker owns only native strings, copied byte
vectors and result records. It never reads JS heap storage or invokes JS. This
keeps the engine single-threaded and requires no Ladybird changes.

EventLoop roots each GC completion function until its native request completes.
Completions build JS results and invoke callbacks on the owning VM thread, then
drain microtasks/rejection checks before the next callback. Active work requests
keep the loop alive. Internal descriptors are closed and every uv_fs_t request is
cleaned up; readFile/writeFile passed a numeric descriptor borrow it without
closing it or resetting its position. Promise file operations accept paths;
FileHandle support is described in ADR 0012. Scalar open/close/read/write, fstat/ftruncate and
fsync/fdatasync use the same native request/completion ownership. Worker writes snapshot input bytes at submission;
applications must not expect mutations after submission to affect pending writes.

The implemented subset is readFile/writeFile/appendFile, access/exists,
stat/lstat, mkdir/rmdir, unlink/rename and readdir, with synchronous, callback and
Promise forms where Node defines them. node:fs/promises shares fs.promises through
the builtin registry. Validate paths/options before dispatch; native errors carry
code, errno, syscall and applicable path/dest. Strings, Uint8Array/Buffer and branded file URL paths
are supported, including NUL rejection. File URL conversion is described in ADR 0013. Numeric descriptors are supported by
callback/synchronous whole-file operations. Only currently required native flag,
access and file-type constants are exposed.

Read files in bounded chunks, handling partial reads/writes. Whole-file reads
are limited to 2 GiB minus one byte, returning EFBIG above that limit rather than
Node's ERR_FS_FILE_TOO_LARGE. The result becomes engine-owned byte storage; Buffer
views retain that storage. Native TypedArray copies also avoid JS per-byte loops.
Returned Stats/BigIntStats retain exact native integer values as decimal strings
until JS conversion, and BigInt timestamps retain nanoseconds. Dirent names can
be strings or Buffers; unknown directory entry types use lstat when requested.
Public constructor edge cases are not claimed covered. Scalar I/O covers
positioned/current-offset access, Buffer/TypedArray/DataView byte ranges, callback
identity and optional argument forms. Read positions support exact signed-64-bit
BigInt values; numeric write positions above the safe-integer range explicitly
reject rather than risking imprecise offsets.

## Fatal termination policy

Do not enter application callbacks during teardown. A fatal error with outstanding
native work flushes standard streams and terminates the process through _Exit.
The OS reclaims workers/handles; an uninterruptible FIFO open/read must not prevent
fatal termination. This is an intentional difference: Node v24.15.0 exceeded a
30-second timeout after an uncaught error with a pending FIFO read on this host.
Pending writes are not guaranteed to complete after fatal termination. A normal
exitCode still waits for referenced work to finish. process.exit remains immediate.

## Evidence and limits

The initial twelve differential workflows cover binary/Unicode data, typed views, append and
exclusive creation, recursive mkdir, returned stat/dirent values, symlinks,
encodings and invalid inputs, OS error fields, CJS/ESM identity, borrowed descriptor
positions/ownership, parallel reads and callback microtask checkpoints. A FIFO
read with a timer writer proves asynchronous I/O does not block the VM thread.
Fatal-callback tests compare status/stdout and diagnostic markers, not stacks.
Two additional Mite-only FIFO cases cover the intentional fatal-exit policy; one
fixture checks explicit unsupported options. Tests compare final file contents
in isolated directories as well as program output. FIFO/descriptor inheritance
checks are POSIX-specific and report unverified coverage on other hosts.

Vector I/O, streams,
watching, permissions, copy/rm/link services and other fs APIs remain absent.
AbortSignal, flush, recursive readdir/rmdir explicitly reject as unsupported.
Legacy existsSync deprecation warnings are not emitted. Error wording/stacks and
full Node event-loop phase ordering are not claimed equivalent. Native Linux and
Windows execution, including their path/permission behavior, remains unverified.

References: [Node 24 filesystem](https://nodejs.org/docs/latest-v24.x/api/fs.html),
[libuv filesystem](https://docs.libuv.org/en/v1.x/fs.html).
