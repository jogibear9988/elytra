import fs from 'node:fs';
import * as p from 'node:fs/promises';
import assert from 'node:assert/strict';
import { AsyncLocalStorage } from 'node:async_hooks';
import { join } from 'node:path';
const root = fs.mkdtempSync(join(process.cwd(), 'fs-primitives-'));

async function remove(value) {
  const stat = await p.lstat(value);
  if (stat.isDirectory()) {
    for (const name of await p.readdir(value)) {
      await remove(join(value, name));
    }
    await p.rmdir(value);
  } else {
    await p.unlink(value);
  }
}

try {
  const file = join(root, 'native-ü.txt');
  const hard = join(root, 'hard');
  const sym = join(root, 'sym');
  fs.writeFileSync(file, 'actual native bytes');
  fs.linkSync(file, hard);
  assert.equal(fs.statSync(file).ino, fs.statSync(hard).ino);
  await p.unlink(hard);
  await p.link(file, hard);
  assert.equal((await p.stat(file)).ino, (await p.stat(hard)).ino);
  fs.symlinkSync('native-ü.txt', sym, 'file');
  assert.equal(fs.readlinkSync(sym), 'native-ü.txt');
  assert.deepEqual(await p.readlink(sym, { encoding: 'buffer' }), Buffer.from('native-ü.txt'));
  assert.equal((await p.lstat(sym)).isSymbolicLink(), true);
  fs.utimesSync(file, new Date(1000000), '2000');
  assert.equal(Math.trunc(fs.statSync(file).mtimeMs), 2000000);
  await p.utimes(file, 3000, new Date(4000000));
  assert.equal(Math.trunc((await p.stat(file)).mtimeMs), 4000000);
  await assert.rejects(
    p.link(file, hard),
    (error) => error.code === 'EEXIST' && error.dest === hard
  );
  console.log('PASS native links readlink and timestamps');
  const syncDir = fs.opendirSync(root, { bufferSize: 1 });
  const names = [];
  for (;;) {
    const entry = syncDir.readSync();
    if (!entry) {
      break;
    }
    names.push(entry.name);
  }
  syncDir.closeSync();
  assert.deepEqual(names.sort(), ['hard', 'native-ü.txt', 'sym']);

  assert.throws(() => syncDir.readSync(), { code: 'ERR_DIR_CLOSED' });

  const directory = await p.opendir(root, { bufferSize: 1, encoding: 'buffer' });
  const streamed = [];
  for await (const entry of directory) {
    assert.ok(Buffer.isBuffer(entry.name));
    streamed.push(entry.name.toString());
  }
  assert.deepEqual(streamed.sort(), names.sort());
  await assert.rejects(directory.read(), { code: 'ERR_DIR_CLOSED' });
  const local = new AsyncLocalStorage();
  const queued = await p.opendir(root, { bufferSize: 1 });
  await Promise.all(
    ['first', 'second'].map((value) =>
      local.run(
        value,
        () =>
          new Promise((resolve, reject) =>
            queued.read((error, entry) => {
              try {
                assert.equal(error, null);
                assert.equal(local.getStore(), 'first');
                assert.ok(entry);
                resolve();
              } catch (error) {
                reject(error);
              }
            })
          )
      )
    )
  );
  await queued.close();
  await assert.rejects(p.opendir(join(root, 'missing')), { code: 'ENOENT' });
  console.log('PASS actual directory streams iteration close and queued context');
  const source = join(root, 'source');
  const target = join(root, 'target');
  await p.mkdir(join(source, 'nested'), { recursive: true });
  await p.writeFile(join(source, 'nested', 'bytes.bin'), Buffer.from([0, 255, 17]));
  await p.symlink('nested/bytes.bin', join(source, 'link'));
  await p.cp(source, target, { recursive: true, preserveTimestamps: true, verbatimSymlinks: true });
  assert.deepEqual(
    await p.readFile(join(target, 'nested', 'bytes.bin')),
    Buffer.from([0, 255, 17])
  );
  assert.equal(await p.readlink(join(target, 'link')), 'nested/bytes.bin');
  await p.writeFile(join(target, 'nested', 'bytes.bin'), 'keep');
  await p.cp(source, target, { recursive: true, force: false, verbatimSymlinks: true });
  assert.equal(await p.readFile(join(target, 'nested', 'bytes.bin'), 'utf8'), 'keep');
  await assert.rejects(p.cp(source, join(source, 'self'), { recursive: true }));
  assert.equal(fs.existsSync(join(source, 'self')), false);
  await p.cp(source, join(root, 'filtered'), {
    recursive: true,

    filter: async (src) => !src.endsWith('link')
  });
  assert.equal(fs.existsSync(join(root, 'filtered', 'link')), false);
  console.log(
    'PASS genuine bounded recursive copy files symlinks filter overwrite and self-refusal'
  );
} finally {
  await remove(root);
}
