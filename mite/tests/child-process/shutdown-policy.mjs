/** Owned-policy fault injection supplements, and does not replace, native pipe acceptance. */
import create from '../../modules/process/child-process.mjs';
import * as stream from 'node:stream';
import * as events from 'node:events';
import { Buffer } from 'node:buffer';
import { fileURLToPath } from 'node:url';

for (const mode of ['disconnected', 'async-cancelled', 'io-error']) {
  let notify;
  const native = {
    spawn(spec, callback) {
      notify = callback;
      return { id: 1, pid: 1, handles: 1, status: 0 };
    },

    shutdown(id, callback) {
      if (mode === 'async-cancelled') {
        queueMicrotask(() => callback(-3));

        return 0;
      }
      return mode === 'disconnected' ? -1 : -2;
    },

    closePipe() {
      queueMicrotask(() => notify('pipeClose', 0));
    },

    error(status) {
      return { code: status === -1 ? 'ENOTCONN' : 'EIO', message: 'owned injected status' };
    }
  };
  const cp = create({
    native,
    stream,
    events,
    Buffer,
    process,
    URL,
    fileURLToPath,
    platform: process.platform,
    signals: { SIGTERM: 15 }
  });
  const child = cp.spawn('owned-policy-fixture', [], { stdio: ['pipe', 'ignore', 'ignore'] });
  const errors = [];

  child.stdin.on('error', (error) => errors.push([error.code, error.syscall]));

  const closed = new Promise((resolve) => child.stdin.on('close', resolve));
  child.stdin.end();
  await closed;
  const expected = mode === 'io-error' ? [['EIO', 'shutdown']] : [];
  if (
    JSON.stringify(errors) !== JSON.stringify(expected) ||
    child.stdin.writableFinished !== (mode !== 'io-error')
  ) {
    throw new Error(JSON.stringify({ mode, errors, finished: child.stdin.writableFinished }));
  }
}
console.log('PASS owned shutdown ENOTCONN, async completion and preserved synchronous EIO');
