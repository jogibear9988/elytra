/** Use the actual filesystem, resolver, child process and native Promise timers. */
import { promisify } from 'node:util';
import fs from 'node:fs';
import dns from 'node:dns';
import { execFile } from 'node:child_process';
import { setTimeout as timeout, setImmediate as immediate } from 'node:timers';
import * as timerPromises from 'node:timers/promises';
import { Buffer } from 'node:buffer';

function check(value, label) {
  if (!value) {
    throw new Error(label);
  }
}

for (const fn of [fs.read, fs.write, dns.lookup]) {
  check(fn[promisify.custom] === undefined, 'named arguments use private metadata');
  check(promisify(fn) !== promisify(fn), 'named-result wrapper identity');
}
const filename = process.argv[2];
const childScript = process.argv[3];
await promisify(fs.writeFile)(filename, 'hello');
check((await promisify(fs.readFile)(filename, 'utf8')) === 'hello', 'real file read/write');
const fd = await promisify(fs.open)(filename, 'r+');
try {
  const buffer = Buffer.alloc(5);
  const read = await promisify(fs.read)(fd, buffer, 0, 5, 0);
  check(
    read.bytesRead === 5 && read.buffer === buffer && buffer.toString() === 'hello',
    'read named fields'
  );
  const write = await promisify(fs.write)(fd, 'world', 0, 'utf8');
  check(write.bytesWritten === 5 && write.buffer === 'world', 'write named fields');
  check((await promisify(fs.fstat)(fd)).size === 5, 'real descriptor stat');
} finally {
  await promisify(fs.close)(fd);
}
try {
  await promisify(fs.readFile)(filename + '.missing');
  throw new Error('missing file accepted');
} catch (error) {
  check(error.code === 'ENOENT', 'native file failure');
}
const address = await promisify(dns.lookup)('127.0.0.1');
check(address.address === '127.0.0.1' && address.family === 4, 'lookup named fields');
const all = await promisify(dns.lookup)('127.0.0.1', { all: true });
check(all.length === 1 && all[0].address === '127.0.0.1', 'lookup all values');
check(
  promisify(timeout) === timerPromises.setTimeout &&
    promisify(immediate) === timerPromises.setImmediate,
  'timer custom identity'
);
check((await promisify(timeout)(1, 'timer')) === 'timer', 'native Promise timeout');
check((await promisify(immediate)('immediate')) === 'immediate', 'native Promise immediate');
const controller = new AbortController();
const cancelled = promisify(timeout)(10000, 'unused', { signal: controller.signal });
controller.abort('stop');
try {
  await cancelled;
  throw new Error('cancel ignored');
} catch (error) {
  check(
    error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === 'stop',
    'native timer cancellation'
  );
}
console.log(
  'PASS real filesystem/descriptors, named read/write and DNS results, Promise timers and cancellation'
);
const pending = promisify(execFile)(process.execPath, [childScript]);
check(typeof pending.child?.pid === 'number', 'child available before completion');
const output = await pending;
check(
  output.stdout === 'child output\n' && output.stderr === 'child error\n',
  'child output fields'
);
try {
  await promisify(execFile)(process.execPath, [childScript, 'fail']);
  throw new Error('child failure ignored');
} catch (error) {
  check(
    error.code === 7 && error.stdout === 'child output\n' && error.stderr === 'child error\n',
    'child rejection fields'
  );
}
console.log('PASS real execFile Promise child, stdout/stderr and nonzero exit rejection');
