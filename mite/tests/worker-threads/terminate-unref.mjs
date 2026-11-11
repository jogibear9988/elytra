/** No timer/other referenced handle may hide terminate's required worker re-reference. */
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';
const worker = new Worker(new URL('./bare-loop.mjs', import.meta.url));
await new Promise((resolve, reject) => {
  worker.once('online', resolve);
  worker.once('error', reject);
});
worker.unref();
assert.equal(await worker.terminate(), 1);
console.log('PASS terminate re-references an unref worker until joined exit settles');
