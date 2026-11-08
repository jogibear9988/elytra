/** Force short actual fs writes, then prove ending/destroying wrappers keeps fd 1/2 alive. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { Writable } from 'node:stream';
import install from '../../modules/core/process-stdio.mjs';
let calls = 0;
const borrowed = {};
install(
  {
    Writable,
    fs: {
      write(fd, bytes, offset, length, position, callback) {
        calls++;
        fs.write(fd, bytes, offset, Math.min(length, 3), position, callback);
      }
    }
  },
  borrowed
);

assert.throws(() => borrowed.stdin, { code: 'ERR_MITE_UNSUPPORTED_API' });

await new Promise((resolve, reject) =>
  borrowed.stdout.end('partial Grüße 🦋\n', (error) => (error ? reject(error) : resolve()))
);
assert.ok(calls > 2);
borrowed.stdout.destroy();
await new Promise((resolve, reject) =>
  fs.write(1, 'after end/destroy\n', (error) => (error ? reject(error) : resolve()))
);
borrowed.stderr.destroy();
await new Promise((resolve, reject) =>
  fs.write(2, 'stderr descriptor alive\n', (error) => (error ? reject(error) : resolve()))
);
const broken = {};
install(
  {
    Writable,
    fs: {
      write(fd, bytes, offset, length, position, callback) {
        callback(null, 0);
      }
    }
  },
  broken
);
const error = await new Promise((resolve) => {
  broken.stdout.on('error', () => {});

  broken.stdout.write('x', resolve);
});
assert.equal(error.code, 'EIO');
