/** Real worker entry: its heap/global/environment belong only to this native thread. */
import { parentPort, threadId, isMainThread } from 'node:worker_threads';
import process from 'node:process';
if (isMainThread || !parentPort || threadId <= 0) {
  throw new Error('Missing actual worker role');
}

parentPort.on('message', (message) => {
  if (message.op === 'loop') {
    parentPort.postMessage('looping');
    while (true) {
      /* termination must interrupt actual script, not a cooperative timer */
    }
  }
  if (message.op === 'exit') {
    parentPort.postMessage('before-exit');
    process.exit(3);
    throw new Error('Worker process.exit returned');
  }
  if (message.op === 'throw') {
    throw new Error('owned-worker-failure');
  }
  if (message.op === 'echo') {
    const absent = globalThis.parentSecret === undefined;
    globalThis.workerSecret = true;
    process.env.MITE_WORKER_OWNED_ENV = 'child';
    let chdirCode;
    try {
      process.chdir(process.cwd());
    } catch (error) {
      chdirCode = error.code;
    }
    parentPort.postMessage({ pid: process.pid, threadId, absent, chdirCode, value: message.value });
    parentPort.close();
  }
});
