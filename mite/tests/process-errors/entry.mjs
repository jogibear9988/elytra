/** ESM entry failures are fatal exceptions even when rejection listeners exist. */
import assert from 'node:assert/strict';

const error = new Error('controlled-esm-entry');
const events = [];

process.on('unhandledRejection', () => {
  throw new Error('ESM entry incorrectly routed to unhandledRejection');
});

process.on('uncaughtExceptionMonitor', (actual, origin) => {
  assert.equal(actual, error);
  assert.equal(origin, 'unhandledRejection');
  events.push('monitor');
});

process.once('uncaughtException', (actual, origin) => {
  assert.equal(actual, error);
  assert.equal(origin, 'unhandledRejection');
  assert.deepEqual(events, ['monitor']);

  setTimeout(() => {
    console.log(
      `PASS process ESM entry ${process.argv[2]} uses uncaught exception before rejection listeners`
    );
  }, 0);
});

if (process.argv[2] === 'await') {
  await Promise.reject(error);
} else {
  throw error;
}
console.log('FORBIDDEN entry continuation');
