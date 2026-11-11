# File copying

`fs.copyFile`, `fs.copyFileSync` and `fs.promises.copyFile` use genuine
`uv_fs_copyfile` operations. Source and destination are validated path strings,
Buffers or file URLs. Async calls copy their native argument bytes before worker
admission and complete on the VM owner thread with the originating async context.
Success returns `undefined`; callback success is `callback(null)`.

Modes combine `COPYFILE_EXCL`, `COPYFILE_FICLONE` and
`COPYFILE_FICLONE_FORCE`. Clone support depends on the actual OS/filesystem;
forced cloning reports the genuine native failure instead of silently falling
back. Copying is not atomic, and a failed native copy can remove its destination.
Errors include native `code`, `errno`, `syscall: 'copyfile'`, `path` and `dest`.
This API does not provide recursive directory copying or cancellation.

The focused fixture `mite/tests/copy-file/main.mjs` covers binary contents,
overwriting, exclusion, clone fallback/forced failure, URL/Buffer/Unicode paths,
async callback ordering/context, promise delivery and argument/native errors.
