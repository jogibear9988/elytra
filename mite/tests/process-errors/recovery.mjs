/** Real callback failures retain identity and leave later owned work runnable. */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs';
import collect from './collect.mjs';

const events = [];
const phases = ['timer', 'immediate', 'microtask', 'nextTick', 'io'];
let phase = 0;
let expected;

function removed() {
  throw new Error('removed listener ran');
}

process.on('uncaughtException', removed);
process.off('uncaughtException', removed);

process.on('uncaughtExceptionMonitor', function (error, origin) {
  collect?.();
  assert.equal(this, process);
  assert.equal(error, expected);
  assert.equal(origin, 'uncaughtException');
  events.push('monitor');
});

process.prependListener('uncaughtExceptionMonitor', () => events.push('monitor-prepend'));

process.once('uncaughtException', () => events.push('once'));

process.prependOnceListener('uncaughtException', () => events.push('prepend-once'));

process.on('uncaughtException', function (error, origin) {
  collect?.();
  assert.equal(this, process);
  assert.equal(error, expected);
  assert.equal(origin, 'uncaughtException');
  assert.deepEqual(events, [
    'monitor-prepend',
    'monitor',
    ...(phase === 0 ? ['prepend-once', 'once'] : [])
  ]);
  events.length = 0;
  const completed = phases[phase++];
  // Both Promise jobs and actual native file completions must survive recovery.
  Promise.resolve().then(() => {
    readFile(new URL(import.meta.url), (failure, bytes) => {
      assert.ifError(failure);
      assert.ok(bytes.length > 0);
      console.log(`PASS process uncaught ${completed} monitor ordering identity and recovery`);
      if (phase === phases.length) {
        assert.equal(process.listenerCount('uncaughtException'), 1);
        process.removeAllListeners('uncaughtException');
        process.removeAllListeners('uncaughtExceptionMonitor');
      } else {
        setTimeout(trigger, 0);
      }
    });
  });
});

function trigger() {
  expected = new Error(`controlled-${phases[phase]}`);

  const fail = () => {
    throw expected;
  };

  switch (phases[phase]) {
    case 'timer':
      setTimeout(fail, 0);
      break;
    case 'immediate':
      setImmediate(fail);
      break;
    case 'microtask':
      queueMicrotask(fail);
      break;
    case 'nextTick':
      process.nextTick(fail);
      break;
    case 'io':
      readFile(new URL(import.meta.url), (failure) => {
        assert.ifError(failure);
        fail();
      });
      break;
  }
}

setTimeout(trigger, 0);
