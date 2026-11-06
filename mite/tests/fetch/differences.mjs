/** Preserve observed Node differences; do not silently normalize them into parity. */
import { setTimeout as sleep } from 'node:timers/promises';

const base = `http://127.0.0.1:${process.argv[2]}`;
try {
  const response = await fetch(base + '/compressed');
  console.log('compression decoded', await response.text());
} catch (error) {
  if (error.cause?.code !== 'ERR_MITE_UNSUPPORTED_API') {
    throw error;
  }
  console.log('compression rejected', error.cause.code);
}

const control = new AbortController();

control.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

const response = await fetch(base + '/body/suppressed', { signal: control.signal });
const reader = response.body.getReader();
let count = 0;
while (count < 4096) {
  count += (await reader.read()).value.byteLength;
}
const reason = new Error('private abort');
control.abort(reason);
const next = reader.read().then(
  (value) => ({ value }),
  (error) => ({ error })
);
const outcome = await Promise.race([next, sleep(20, { pending: true })]);
if (outcome.error === reason) {
  console.log('suppressed public event aborted');
} else if (outcome.pending) {
  console.log('suppressed public event pending');
} else {
  throw new Error('unexpected signal outcome');
}
await reader.cancel().catch((error) => {
  if (error !== reason) {
    throw error;
  }
});
await next;
await (await fetch(base + '/release/suppressed')).text();
let cancelled = false;
let finishPull;
const upload = new ReadableStream({
  start(controller) {
    controller.enqueue(new Uint8Array(4096));
  },

  pull(controller) {
    return new Promise((resolve) => {
      finishPull = () => {
        if (!cancelled) {
          controller.close();
        }
        resolve();
      };
    });
  },

  cancel() {
    cancelled = true;
  }
});
const early = await fetch(base + '/early', { method: 'POST', body: upload, duplex: 'half' });
if (early.status !== 413 || (await early.text()) !== 'early') {
  throw new Error('invalid early response');
}
await sleep(20);
console.log('early upload cancelled', cancelled);
finishPull?.();
console.log('DONE Fetch differences');
