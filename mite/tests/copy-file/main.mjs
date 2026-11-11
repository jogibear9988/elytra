import fs from 'node:fs';
import { copyFile } from 'node:fs/promises';
import { AsyncLocalStorage } from 'node:async_hooks';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
const root = fs.mkdtempSync(join(process.cwd(), 'copy-file-'));
try {
  const src = join(root, 'source-ü.bin');
  const dest = join(root, 'destination-ß.bin');
  const bytes = Buffer.from(Array.from({ length: 65537 }, (_, i) => i % 251));
  fs.writeFileSync(src, bytes);
  assert.equal(fs.constants.COPYFILE_EXCL, 1);
  assert.equal(fs.constants.COPYFILE_FICLONE, 2);
  assert.equal(fs.constants.COPYFILE_FICLONE_FORCE, 4);
  assert.equal(fs.copyFileSync(pathToFileURL(src), Buffer.from(dest)), undefined);
  assert.deepEqual(fs.readFileSync(dest), bytes);
  fs.writeFileSync(dest, 'overwrite');
  fs.copyFileSync(src, dest);
  assert.deepEqual(fs.readFileSync(dest), bytes);
  for (const mode of [1, 3, 5, 7]) {
    assert.throws(
      () => fs.copyFileSync(src, dest, mode),
      (e) => e.code === 'EEXIST' && e.syscall === 'copyfile' && e.path === src && e.dest === dest
    );
  }
  const local = new AsyncLocalStorage();
  let returned = false;
  const sourcePath = Buffer.from(src);
  const destinationPath = Buffer.from(dest);
  const operation = local.run(
    'copy',
    () =>
      new Promise((resolve, reject) => {
        fs.copyFile(sourcePath, destinationPath, (error) => {
          try {
            assert.equal(returned, true);
            assert.equal(error, null);
            assert.equal(local.getStore(), 'copy');
            resolve();
          } catch (error) {
            reject(error);
          }
        });
      })
  );
  sourcePath.fill(0);
  destinationPath.fill(0);
  returned = true;
  await operation;
  assert.equal(await copyFile(src, dest, fs.constants.COPYFILE_FICLONE), undefined);
  assert.deepEqual(fs.readFileSync(dest), bytes);
  // Forced clone genuinely succeeds or reports this filesystem's native error.
  try {
    await copyFile(src, dest, fs.constants.COPYFILE_FICLONE_FORCE);
  } catch (error) {
    assert.ok(['ENOTSUP', 'EOPNOTSUPP', 'ENOSYS', 'EXDEV'].includes(error.code));
  }
  await copyFile(src, dest); // Failed forced-clone operations may remove the destination.
  await assert.rejects(
    copyFile(join(root, 'absent'), dest),
    (e) => e.code === 'ENOENT' && e.syscall === 'copyfile' && e.dest === dest
  );
  assert.deepEqual(fs.readFileSync(dest), bytes);
  for (const mode of [-1, 8, NaN, Infinity]) {
    assert.throws(() => fs.copyFileSync(src, dest, mode), { code: 'ERR_OUT_OF_RANGE' });
  }
  for (const mode of ['1', {}, true]) {
    assert.throws(() => fs.copyFileSync(src, dest, mode), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  assert.throws(() => fs.copyFileSync(src + '\0', dest), { code: 'ERR_INVALID_ARG_VALUE' });

  await copyFile(src, dest, null);

  assert.throws(() => fs.copyFileSync(src, dest, 1.2), { code: 'EEXIST' });
} finally {
  for (const name of fs.readdirSync(root)) {
    fs.unlinkSync(join(root, name));
  }
  fs.rmdirSync(root);
}
console.log('PASS real copyFile sync callback promise bytes flags errors and context');
