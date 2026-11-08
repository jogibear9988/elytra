/** Real Mite -> public factory -> exact native Host -> controlled ownership provider. */
import native from 'power-save-test';
import teardown from 'power-save-teardown-test';
import createPowerSaveBlocker from '../../core/power-save-blocker/index.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function throws(callback, code, message) {
  let caught;
  try {
    callback();
  } catch (error) {
    caught = error;
  }
  check(caught?.code === code, message + ': ' + caught);
  return caught;
}

const failure = (code, message) => Object.assign(new Error(message), { code });

const component = createPowerSaveBlocker(
  native,
  () => false,
  failure,
  (name) => {
    throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
  }
);
const blocker = component.powerSaveBlocker;

const stats = () => native.counts().split(',').map(Number);

check(stats().join() === '0,0,0,0,0,0', 'Import never constructs a provider');
for (const id of [-2147483648, -1, 0, 2147483647]) {
  check(blocker.stop(id) === false && blocker.isStarted(id) === false, 'Unknown IDs are false');
  check(native.powerSaveBlockerStop(id) === false, 'Direct native unknown ID');
}
for (const value of [
  undefined,
  null,
  true,
  '0',
  {},
  new Number(0),
  1n,
  0.5,
  NaN,
  Infinity,
  -Infinity,
  -0
]) {
  for (const callback of [
    () => blocker.stop(value),
    () => blocker.isStarted(value),
    () => native.powerSaveBlockerStop(value),
    () => native.powerSaveBlockerIsStarted(value)
  ]) {
    throws(callback, 'ERR_INVALID_ARG_TYPE', 'Strict signed-int32 input');
  }
}
for (const value of [-2147483649, 2147483648]) {
  throws(() => blocker.stop(value), 'ERR_OUT_OF_RANGE', 'Public ID range');

  throws(() => native.powerSaveBlockerIsStarted(value), 'ERR_OUT_OF_RANGE', 'Native ID range');
}

throws(() => native.powerSaveBlockerStart(null), 'ERR_INVALID_ARG_TYPE', 'Native type validation');

throws(() => blocker.start('unknown'), 'ERR_INVALID_ARG_VALUE', 'Public type validation');

throws(
  () => native.powerSaveBlockerStart('unknown'),
  'ERR_INVALID_ARG_VALUE',
  'Native type validation'
);

check(stats()[0] === 0, 'Invalid/unknown requests leave provider uncreated');
native.control(1);
const error = throws(
  () => blocker.start('prevent-app-suspension'),
  'ERR_CONTROLLED_NATIVE',
  'Start error'
);
check(error.message === 'Native failure: Grüße 日本語', 'Native UTF-8 message preserved');
const id = blocker.start('prevent-display-sleep');
check(id === 0, 'Zero is a valid first native request ID');
native.collect();
check(blocker.isStarted(id), 'Native ownership survives actual GC');
native.control(4);

throws(() => blocker.isStarted(id), 'ERR_CONTROLLED_NATIVE', 'Query error preserved');

native.control(3);

throws(() => blocker.stop(id), 'ERR_CONTROLLED_NATIVE', 'Stop error preserved');

check(blocker.isStarted(id), 'Failed release preserves ownership');
check(blocker.stop(id) && !blocker.stop(id), 'Stop succeeds exactly once');
const beforeNegative = stats().join();
check(
  !blocker.stop(-1) && !blocker.isStarted(-1),
  'Negative IDs stay unknown after provider creation'
);
check(stats().join() === beforeNegative, 'Negative IDs never reach provider');
native.control(2);

throws(
  () => blocker.start('prevent-app-suspension'),
  'ERR_FIREFLY_POWER_SAVE_DATA',
  'Corrupt admission rejected'
);

check(stats()[4] === 1 && stats()[5] === 0, 'Corrupt admission revokes owned requests');

throws(
  () => blocker.start('prevent-app-suspension'),
  'ERR_FIREFLY_APP_EXITED',
  'Native service stays revoked'
);

native.powerSaveBlockerShutdown();
component.stop();
component.stop();
check(stats()[4] === 1, 'Repeated shutdown remains idempotent');

throws(() => blocker.isStarted(0), 'ERR_FIREFLY_APP_EXITED', 'Public shutdown terminal');

const survivor = createPowerSaveBlocker(
  teardown,
  () => false,
  failure,
  () => {}
);
check(
  survivor.powerSaveBlocker.start('prevent-app-suspension') === 0,
  'Second controller owns its own ID'
);
native.collect();
check(stats()[5] === 1, 'Leave one genuine controlled ownership request for host teardown');
console.log(
  'PASS real factory/exact Host lazy admission, strict IDs, UTF-8 failures, GC and terminal cleanup'
);
