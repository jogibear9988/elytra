/** Nonzero signals are sent only to the parent's authenticated, exclusively owned victim. */
import assert from 'node:assert/strict';
import processDefault, { kill } from 'node:process';
import fs from 'node:fs';
import { constants } from 'node:os';
import { createRequire } from 'node:module';
const [targetText, parentText, nonce, mode] = process.argv.slice(2);
const target = Number(targetText);
const parent = Number(parentText);
assert.ok(Number.isInteger(target) && target > 0 && target !== process.pid && target !== parent);
assert.equal(processDefault, process);
assert.equal(kill, process.kill);
assert.equal(createRequire(import.meta.url)('node:process').kill, kill);
assert.equal(parent, process.ppid);
assert.equal(kill(process.pid, 0), true);
assert.equal(kill(process.pid, -0), true);
assert.equal(kill(parent, 0), true);
assert.equal(kill(target, 0), true);
// The runner creates this runtime in its own session/group. No desktop group
// receives a signal: these probes are strictly numeric signal zero.
if (process.platform !== 'win32') {
  assert.equal(kill(0, 0), true);
  assert.equal(kill(-process.pid, 0), true);
  assert.equal(kill(false, 0), true);
}
assert.equal(kill(String(target), 0), true);
for (const pid of [
  undefined,
  null,
  1.5,
  NaN,
  Infinity,
  -Infinity,
  2147483648,
  -2147483649,
  'not-a-pid'
]) {
  assert.throws(() => kill(pid, 0), { code: 'ERR_INVALID_ARG_TYPE' });
}
for (const pid of [1n, Symbol('pid')]) {
  assert.throws(() => kill(pid, 0), TypeError);
}
const sentinel = { owned: 'coercion' };

assert.throws(
  () =>
    kill(
      {
        valueOf() {
          throw sentinel;
        }
      },
      0
    ),
  (error) => error === sentinel
);

for (const signal of ['OWNED_UNKNOWN_SIGNAL', 'sigterm', 1.5, 2147483648]) {
  assert.throws(() => kill(target, signal), { code: 'ERR_UNKNOWN_SIGNAL' });
}

assert.throws(
  () =>
    kill(target, {
      toString() {
        throw sentinel;
      }
    }),
  (error) => error === sentinel
);

for (const signal of [0n, Symbol('signal')]) {
  assert.throws(() => kill(target, signal), TypeError);
}
assert.equal(kill(target, 0), true, 'Zero and rejected probes must not default to SIGTERM');
process.stdout.write(JSON.stringify({ phase: 'ready', target, pid: process.pid, nonce }) + '\n');

async function gate(expected) {
  const byte = Buffer.alloc(1);
  for (let attempt = 0; attempt < 1000; attempt++) {
    try {
      assert.equal(fs.readSync(0, byte, 0, 1, null), 1);
      assert.equal(byte[0], expected.charCodeAt(0));
      return;
    } catch (error) {
      if (error.code !== 'EAGAIN' && error.code !== 'EWOULDBLOCK') {
        throw error;
      }
    }
    await new Promise((done) => setTimeout(done, 5));
  }
  throw Error('Owned process signal gate deadline');
}

await gate('g');
assert.equal(kill(target, 0), true);
let success;
switch (mode) {
  case 'default':
    success = kill(target);
    break;
  case 'named':
    success = kill(target, 'SIGTERM');
    break;
  case 'numeric':
    success = kill(target, constants.signals.SIGTERM);
    break;
  case 'false':
    success = kill(target, false);
    break;
  case 'null':
    success = kill(target, null);
    break;
  case 'empty':
    success = kill(target, '');
    break;
  default:
    throw Error('Unknown owned signal mode');
}
assert.equal(success, true);
process.stdout.write(JSON.stringify({ phase: 'sent', target, nonce }) + '\n');
// The parent releases this gate only after a real wait/reap of the victim.
await gate('r');

assert.throws(
  () => kill(target, 0),
  (error) => {
    assert.equal(error.code, 'ESRCH');
    assert.equal(typeof error.errno, 'number');
    assert.ok(error.errno < 0);
    assert.equal(error.syscall, 'kill');
    return true;
  }
);

process.stdout.write('PASS process.kill owned OS delivery, coercion, zero and reaped ESRCH\n');
