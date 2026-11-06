/** Abort a real native Promise timer through a fixed controller signal. */
import { setTimeout as sleep } from 'node:timers/promises';
const controller = new AbortController();
const task = sleep(10000, 'completed', { signal: controller.signal });

setImmediate(() => controller.abort(new Error('Stop requested')));

try {
  await task;
} catch (error) {
  console.log(error.name, error.code, error.cause.message);
}
