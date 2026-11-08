/** Real native FileHandle ranges, stream pressure/ownership and readline integration. */
import { open } from 'node:fs/promises';
import { readFileSync } from 'node:fs';
import { createInterface } from 'node:readline';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const [filename] = process.argv.slice(2);
const bytes = readFileSync(filename);
const fh = await open(filename, 'r');
const ranged = fh.createReadStream({ start: 3, end: 10003, highWaterMark: 101 });
const close = new Promise((resolve, reject) => {
  ranged.once('close', resolve);
  ranged.once('error', reject);
});
const chunks = [];
for await (const chunk of ranged) {
  chunks.push(chunk);
}
await close;
check(Buffer.concat(chunks).equals(bytes.subarray(3, 10004)), 'real file range');
check(ranged.bytesRead === 10001 && fh.fd === -1, 'count and auto close');
const owner = await open(filename, 'r');
const retained = owner.createReadStream({ start: 0, end: 9, autoClose: false });
for await (const chunk of retained) {
  check(chunk.equals(bytes.subarray(0, 10)), 'retained fd data');
}
check(owner.fd >= 0, 'autoClose:false ownership');
retained.destroy();
await owner.close();
const file = await open(filename, 'r');
const source = file.createReadStream({ highWaterMark: 4096 });
const ended = new Promise((resolve, reject) => {
  source.once('close', resolve);
  source.once('error', reject);
});
const rl = createInterface({ input: source, crlfDelay: Infinity });
let count = 0;
const gc = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
for await (const line of rl) {
  check(line === 'Unicode ä 😀 ' + 'x'.repeat(70), 'file line');
  count++;
  if (count % 1024 === 0) {
    gc?.();
  }
}
await ended;
check(count === 30000 && file.fd === -1, 'all real file lines and descriptor retirement');
const cancelOwner = await open(filename, 'r');
const cancelled = cancelOwner.createReadStream({ highWaterMark: 1024 });
const finished = new Promise((resolve) => cancelled.once('close', resolve));
cancelled.read(1);
cancelled.destroy();
await finished;
check(cancelOwner.fd === -1, 'cancel closes fd');
const encoded = await open(filename, 'r');
const encodedStream = encoded.createReadStream({
  start: 0,
  end: 299,
  highWaterMark: 7,
  encoding: 'utf8'
});
let text = '';
for await (const part of encodedStream) {
  check(typeof part === 'string', 'encoded stream text');
  text += part;
}
check(text === bytes.subarray(0, 300).toString('utf8'), 'fragmented file encoding');
await encoded.close();
const manual = await open(filename, 'r');
const manualStream = manual.createReadStream();
const manualClosed = new Promise((resolve) => manualStream.once('close', resolve));
manualStream.read(1);
await manual.close();
await manualClosed;
check(manual.fd === -1 && manualStream.destroyed, 'manual handle close during native read');
const explicit = await open(filename, 'r');
const explicitStream = explicit.createReadStream({ autoClose: false });
const explicitClosed = new Promise((resolve) => explicitStream.once('close', resolve));
explicitStream.destroy();
await explicitClosed;
check(explicit.fd === -1, 'destroy closes handle even with autoClose:false');
console.log('PASS real FileHandle streams/ranges/ownership/cancel and 30000 readline file lines');
