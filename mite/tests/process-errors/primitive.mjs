/** Non-Error rejection reasons are wrapped only by the default fatal promotion. */
import assert from 'node:assert/strict';
import collect from './collect.mjs';

const mode = process.argv[2];
const values = { string: 'controlled-primitive', undefined, null: null, number: 42 };
let monitored;

process.on('uncaughtExceptionMonitor', (error, origin) => {
  collect?.();
  assert.ok(error instanceof Error);
  assert.equal(error.code, 'ERR_UNHANDLED_REJECTION');
  assert.equal(origin, 'unhandledRejection');
  assert.ok(error.message.includes(String(values[mode])));
  monitored = error;
});

process.once('uncaughtException', (error, origin) => {
  collect?.();
  assert.equal(error, monitored);
  assert.equal(origin, 'unhandledRejection');

  setTimeout(() => {
    console.log(`PASS process default primitive ${mode} rejection uses ERR_UNHANDLED_REJECTION`);
  }, 0);
});

setTimeout(() => Promise.reject(values[mode]), 0);
