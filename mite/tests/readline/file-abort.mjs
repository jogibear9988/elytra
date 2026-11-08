/** Native descriptor retirement for pre-aborted and active file streams. */
import { open } from 'node:fs/promises';
const filename = process.argv[2] ?? 'AGENTS.md';
for (const before of [true, false]) {
  const handle = await open(filename, 'r');
  const controller = new AbortController();
  if (before) {
    controller.abort(new Error('stop'));
  }
  const stream = handle.createReadStream({ signal: controller.signal, highWaterMark: 1024 });
  let failure;

  stream.on('error', (error) => {
    failure = error;
  });

  const done = new Promise((resolve) => stream.once('close', resolve));
  if (!before) {
    stream.read(1);
    controller.abort(new Error('stop'));
  }
  await done;
  if (failure?.code !== 'ABORT_ERR' || handle.fd !== -1) {
    throw Error('Abort did not retire native descriptor: ' + failure);
  }
  if (process.versions.mite && handle.listenerCount('close') !== 0) {
    throw Error('File stream close listener leaked');
  }
}
console.log('PASS real FileHandle pre/active abort and descriptor/listener retirement');
