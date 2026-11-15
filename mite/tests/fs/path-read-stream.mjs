/** Native path streams in the existing runner's owned filesystem fixture. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { Readable } from 'node:stream';
const expected = fs.readFileSync('fixture.txt');
const events = [];
const input = fs.createReadStream('fixture.txt', { highWaterMark: 2, start: 1, end: 4 });

assert.equal(input.fd, null);
assert.equal(input.pending, true);

input.on('open', () => {
  assert.equal(typeof input.fd, 'number');
  assert.equal(input.pending, false);
  events.push('open');
});

input.on('ready', () => {
  assert.equal(input.pending, false);
  events.push('ready');
});

input.on('close', () => {
  assert.equal(input.fd, null);
  assert.equal(input.pending, true);
  events.push('close');
});

const chunks = [];
for await (const chunk of input) {
  chunks.push(chunk);
}
assert.deepEqual(Buffer.concat(chunks), expected.subarray(1, 5));
assert.equal(input instanceof fs.ReadStream, true);
assert.equal(input instanceof Readable, true);
assert.equal(input.fd, null);
assert.equal(input.pending, true);
assert.deepEqual(events, ['open', 'ready', 'close']);
assert.equal(input.path, 'fixture.txt');
const controller = new AbortController();
controller.abort();
const stopped = fs.createReadStream('must-not-open', { signal: controller.signal });
assert.equal(stopped.pending, true);
let failure;

stopped.on('error', (error) => {
  failure = error;
});

await new Promise((resolve) => stopped.on('close', resolve));
assert.equal(failure.code, 'ABORT_ERR');
assert.equal(stopped.fd, null);
assert.equal(stopped.pending, true);
const missing = fs.createReadStream('owned-missing-read-stream');
let openFailure;

missing.on('error', (error) => {
  openFailure = error;
});

await new Promise((resolve) => missing.on('close', resolve));
assert.equal(openFailure.code, 'ENOENT');
assert.equal(missing.fd, null);
assert.equal(missing.pending, true);
console.log('PASS native path read streams, range, close and pre-abort');
