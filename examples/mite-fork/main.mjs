/** The current runtime forks itself; the same ESM source also runs under Node. */
import { fork } from 'node:child_process';

const worker = fork(new URL('./worker.mjs', import.meta.url));
const results = new Map();

worker.on('error', (error) => {
  console.error(error);
  process.exitCode = 1;
});

worker.on('message', ({ id, result }) => {
  results.set(id, result);
  if (results.size === 3) {
    // All replies arrived. Disconnect flushes pending writes and releases IPC.
    worker.disconnect();
  }
});

worker.on('exit', (code, signal) => {
  if (code !== 0 || signal !== null || results.size !== 3) {
    console.error('Worker failed', code, signal);
    process.exitCode = 1;
    return;
  }
  console.log(JSON.stringify([...results.values()]));
});

for (let id = 0; id < 3; id++) {
  const message = { id, value: id + 1 };
  worker.send(message);
  // send snapshots JSON immediately; this mutation cannot change the job.
  message.value = 999;
}
