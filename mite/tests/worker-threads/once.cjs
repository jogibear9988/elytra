/** Same-process CommonJS one-shot worker, matching proxy-agent's certificate worker shape. */
const { parentPort, isMainThread, threadId } = require('node:worker_threads');
if (!parentPort || isMainThread || threadId <= 0) {
  throw new Error('Missing worker role');
}
parentPort.postMessage({ pid: process.pid, threadId, done: true });
