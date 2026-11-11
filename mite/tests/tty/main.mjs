/** Read-only real descriptors: running on a PTY does not change its settings or read input. */
import assert from 'node:assert/strict';
import tty, { isatty } from 'node:tty';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import process from 'node:process';
const require = createRequire(import.meta.url);
assert.equal(require('tty'), tty);
assert.equal(require('node:tty').isatty, isatty);
let traps = 0;
const bomb = new Proxy(
  {},
  {
    get() {
      traps++;
      throw new Error('descriptor was coerced');
    }
  }
);
for (const fd of [
  undefined,
  null,
  false,
  true,
  '0',
  '2',
  -1,
  1.5,
  NaN,
  Infinity,
  -Infinity,
  2147483648,
  4294967295,
  1n,
  Symbol('fd'),
  bomb
]) {
  assert.equal(Reflect.apply(isatty, bomb, [fd]), false);
}
assert.equal(traps, 0);
assert.equal(isatty(2147483647), false);
assert.equal(isatty(-0), isatty(0));
const fd = fs.openSync(process.argv[2], 'r');
assert.equal(isatty(fd), false);
fs.closeSync(fd);
assert.equal(isatty(fd), false);
const descriptor = Object.getOwnPropertyDescriptor(tty, 'isatty');
assert.equal(descriptor.enumerable, true);
assert.equal(descriptor.configurable, true);
assert.equal(descriptor.writable, true);
assert.equal(isatty.length, 1);
if (process.versions.mite) {
  assert.throws(() => new tty.ReadStream(0), { code: 'ERR_MITE_UNSUPPORTED_API' });

  assert.throws(() => tty.WriteStream(1), { code: 'ERR_MITE_UNSUPPORTED_API' });
}
console.log(JSON.stringify({ descriptors: [isatty(0), isatty(1), isatty(2)], checked: true }));
