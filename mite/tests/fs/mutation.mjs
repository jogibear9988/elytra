/** Owned fixture inputs: fixture.txt and link -> fixture.txt; no recursive deletion. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { chmod, mkdtemp, rm, lstat, writeFile, rmdir } from 'node:fs/promises';

const first = await mkdtemp('owned-');
const second = fs.mkdtempSync('owned-', { encoding: 'buffer' });
try {
  assert.notEqual(first, second.toString());
  assert.match(first, /^owned-.{6}$/);
  assert.equal((await lstat(first)).isDirectory(), true);
  if (process.platform !== 'win32') {
    assert.equal((await lstat(first)).mode & 0o777, 0o700);
  }
  await chmod('fixture.txt', 0o640);
  if (process.platform !== 'win32') {
    assert.equal((await lstat('fixture.txt')).mode & 0o777, 0o640);
  }
  await rm('link');
  assert.equal(fs.readFileSync('fixture.txt', 'utf8'), 'fixture');
  await rm('missing-owned', { force: true });
  await assert.rejects(rm('missing-owned'), { code: 'ENOENT' });
  await assert.rejects(rm(first), { code: 'ERR_FS_EISDIR' });
  await writeFile(`${first}/file`, 'owned');
  await rm(`${first}/file`, { force: true });
  await assert.rejects(chmod('missing-owned', 0o600), { code: 'ENOENT' });
  await assert.rejects(chmod('fixture.txt', null), { code: 'ERR_INVALID_ARG_TYPE' });
  console.log('PASS genuine chmod, secure mkdtemp and nonrecursive file/symlink rm');
} finally {
  await rmdir(first);
  fs.rmdirSync(second);
}
