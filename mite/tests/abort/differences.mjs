/** Explicit alpha differences: event trust metadata and synthetic abort dispatch. */
import { setTimeout as sleep } from 'node:timers/promises';
const controller = new AbortController();

controller.signal.onabort = (event) => console.log('trusted', event.isTrusted);

controller.abort();
const synthetic = new AbortController();
const result = sleep(5, 'finished', { signal: synthetic.signal }).catch(
  (error) => 'cancelled:' + error.cause
);
synthetic.signal.dispatchEvent(new Event('abort'));
console.log(synthetic.signal.aborted, await result);
