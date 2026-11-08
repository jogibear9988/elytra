# fs.realpath and fs.realpathSync

Mite resolves canonical filesystem paths through native uv_fs_realpath. Sync,
callback and promise forms are implemented, including realpath.native and
realpathSync.native. Local Node/Mite tests pass actual symlinks, broken targets,
loops, file URLs, Buffer paths, encodings, callback ordering and native entry-point
isolation. Fresh Linux/Windows native execution remains unverified.

```js
import { realpath } from 'node:fs/promises';

console.log(await realpath('./workspace'));
```

`realpath(path[, options], callback)` returns undefined. Its callback receives
`(error, resolvedPath)` asynchronously; native errors remain failures with their
actual code, errno, syscall and path. Missing callbacks fail synchronously.
`realpathSync(path[, options])` returns the canonical path or throws.
`fs.promises.realpath(path[, options])` resolves or rejects with the same result.

String paths, file URLs and Buffer paths use existing filesystem validation.
The default encoding is utf8; a string options argument or options.encoding selects
another supported Buffer encoding. Encoding buffer returns a Buffer copied from
the actual OS result. Request input and output bytes outlive their native worker
operation, with callbacks rooted through existing filesystem request ownership.

Both ordinary and .native forms use the OS canonicalizer. Unlike Node's ordinary
realpath implementation, Mite has no JavaScript symlink-walking fallback; particular
error syscall/path details can therefore differ from Node's ordinary form. Errors
from .native use syscall realpath. Symlink limits, network filesystems and Windows
junction behavior depend on the native platform. The .native functions remain bound
to the original implementation when the public fs.realpath wrappers are replaced.
A canonical path is a snapshot, not a guarantee that a subsequent open refers to
the same file or stays within a trusted directory.

```sh
python3 tooling/tests/test_realpath.py --mite build/release-mite/bin/mite
```

The test reports symlink cases separately if Windows lacks symlink privilege.
Non-UTF8 filename checks run only where the filesystem accepts such names; local
macOS rejects creating the test name, so that case is explicitly skipped.
See [filesystem compatibility](../compatibility/mite-0.1.md) and
[Node realpath](https://nodejs.org/download/release/v24.15.0/docs/api/fs.html#fsrealpathpath-options-callback).
