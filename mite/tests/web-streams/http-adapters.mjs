/** Independent HTTP peer holds each response after its first block until released. */
import { get, request } from 'node:http';
import { Readable, Writable } from 'node:stream';
import { ReadableStream } from 'node:stream/web';

const base = `http://127.0.0.1:${process.argv[2]}`;

function response(path) {
  return new Promise((resolve, reject) => {
    get(base + path, resolve).on('error', reject);
  });
}

async function discard(path) {
  const reply = await response(path);
  for await (const chunk of reply) {
    /* Consume the control response. */
  }
}

async function initialBlock(reader) {
  let count = 0;
  while (count < 4096) {
    const { value, done } = await reader.read();
    if (done || !value.every((byte) => byte === 97)) {
      throw new Error('invalid first block');
    }
    count += value.byteLength;
  }
  if (count !== 4096) {
    throw new Error('first block exceeded server barrier');
  }
  return count;
}

const reply = await response('/body');
const reader = Readable.toWeb(reply, {
  strategy: { highWaterMark: 16384, size: (value) => value.byteLength }
}).getReader();
let count = await initialBlock(reader);
await discard('/release');
while (true) {
  const { value, done } = await reader.read();
  if (done) {
    break;
  }
  if (!value.every((byte) => byte === 97)) {
    throw new Error('body corruption');
  }
  count += value.byteLength;
}
if (count !== 1048576) {
  throw new Error('truncated body');
}
console.log('PASS independent incremental HTTP response');

let sent = 0;
const upload = new ReadableStream({
  pull(controller) {
    if (sent++ < 16) {
      controller.enqueue(new Uint8Array(16384).fill(97));
    } else {
      controller.close();
    }
  }
});
const outgoing = request(base + '/upload', {
  method: 'POST',
  headers: { 'content-length': 262144 }
});
const answered = new Promise((resolve, reject) => {
  outgoing.on('response', resolve);
  outgoing.on('error', reject);
});
await upload.pipeTo(Writable.toWeb(outgoing));
let text = '';
for await (const chunk of await answered) {
  text += chunk.toString();
}
if (text !== '262144:valid') {
  throw new Error('upload corruption');
}
console.log('PASS independent streamed HTTP upload');

const cancelled = await response('/cancel');
const cancelling = Readable.toWeb(cancelled).getReader();
await initialBlock(cancelling);
await cancelling.cancel(new Error('stop receiving'));
await discard('/cancel-release');
console.log('PASS independent HTTP cancellation');
console.log('DONE HTTP adapters');
