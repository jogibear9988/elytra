/** Real GC, read pressure, cancellation and callback ordering on native handles. */
import { createGzip, createGunzip, gzipSync } from 'node:zlib';
import { setTimeout as sleep } from 'node:timers/promises';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const gc = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const payload = Buffer.alloc(2 * 1048576, 97);
const compressed = gzipSync(payload);
const decoder = createGunzip({ chunkSize: 1024, readableHighWaterMark: 1024 });
let writeDone = false;

decoder.write(compressed, () => {
  writeDone = true;
});

await sleep(30);
check(!writeDone && decoder.readableLength <= 2048, 'bounded output pressure');
let size = 0;
const done = new Promise((resolve, reject) => {
  decoder.on('data', (chunk) => {
    size += chunk.length;
    if (size % 65536 === 0) {
      gc?.();
    }
  });

  decoder.on('end', resolve);
  decoder.on('error', reject);
});
decoder.end();
await done;
check(size === payload.length && writeDone, 'GC retained codec');
const stream = createGzip();
let closeCalls = 0;
await new Promise((resolve) =>
  stream.close(() => {
    closeCalls++;
    resolve();
  })
);
check(closeCalls === 1 && stream.destroyed, 'close callback');
const cancelled = createGunzip({ chunkSize: 64, readableHighWaterMark: 64 });

cancelled.on('error', () => {});

let callbackCalls = 0;

cancelled.write(compressed, () => {
  callbackCalls++;
});

await sleep(10);
cancelled.destroy(new Error('cancelled'));
gc?.();
await sleep(10);
// Node 24 retains this callback while its full readable queue is destroyed.
// Mite explicitly retires the pending callback once to release owned work.
check(callbackCalls === (process.versions.mite ? 1 : 0), 'documented cancellation callback policy');
console.log('PASS zlib read pressure, native GC lifetime, close and cancellation');
