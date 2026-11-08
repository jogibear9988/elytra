/** Real production native Host/LibJS facade; injected providers publish controlled metadata only. */
import * as native from 'policy-host-test';
import create from '../../modules/system/policy-watcher.mjs';
const facade = create(native);
const defs = {
  Text: { type: 'string' },
  Count: { type: 'number' },
  Enabled: { type: 'boolean' },
  Mixed: { type: ['string', 'boolean', 'number'] }
};
const wire = JSON.stringify(defs);

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

function fails(action, code) {
  let caught;
  try {
    action();
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
}

const turn = () => new Promise((resolve) => setTimeout(resolve, 30));

const stats = () => JSON.parse(native.stats());

const scenario = process.argv[process.argv.indexOf('--scenario') + 1] ?? 'main';

if (process.argv.includes('--leave-active')) {
  function leaveOwned() {
    const callback = () => {
      throw Error('Unqueued shutdown callback must not run');
    };

    native.track(callback);
    return native.start('controlled.shutdown', wire, callback);
  }

  leaveOwned();
  console.log('PASS outstanding policy watcher left for exact native owner shutdown');
} else if (process.argv.includes('--expect-failure')) {
  const index = stats().created;
  const callback =
    scenario === 'callback-throw'
      ? () => {
          throw Error('CONTROLLED_POLICY_CALLBACK_THROW');
        }
      : () => {
          throw Error('Malformed/read error cannot publish successful policy data');
        };
  native.start('controlled.failure', wire, callback);
  const step = {
    'read-error': 3,
    'unknown-key': 4,
    'wrong-type': 5,
    'invalid-utf8': 6,
    'non-finite': 7,
    oversize: 8,
    'callback-throw': 0
  }[scenario];
  check(step !== undefined, 'Unknown fatal scenario');
  native.feed(index, step);
} else {
  const watchdog = setTimeout(() => {
    throw Error('Native policy Host acceptance timed out');
  }, 6000);
  const created = stats().created;
  for (const [product, definitions, callback, code] of [
    [1, wire, () => {}, 'ERR_INVALID_ARG_TYPE'],
    ['product', {}, () => {}, 'ERR_INVALID_ARG_TYPE'],
    ['product', wire, null, 'ERR_INVALID_ARG_TYPE'],
    ['', wire, () => {}, 'ERR_MITE_POLICY_LIMIT'],
    ['bad\0name', wire, () => {}, 'ERR_INVALID_ARG_VALUE'],
    ['bad\ud800', wire, () => {}, 'ERR_INVALID_ARG_VALUE'],
    ['product', '{"Text":{"type":"object"}}', () => {}, 'ERR_INVALID_ARG_VALUE'],
    ['product', '{"Text":{"type":["string","string"]}}', () => {}, 'ERR_INVALID_ARG_VALUE'],
    ['product', '{"Text":{"type":"string","extra":true}}', () => {}, 'ERR_INVALID_ARG_VALUE']
  ]) {
    fails(() => native.start(product, definitions, callback), code);
  }
  check(stats().created === created, 'Malformed native inputs do not create providers');
  native.control(1);

  fails(() => native.start('controlled.unsupported', wire, () => {}), 'ERR_MITE_UNSUPPORTED_API');

  native.control(2);

  fails(() => native.start('controlled.failed', wire, () => {}), 'ERR_MITE_POLICY_PROVIDER');

  const updates = [];
  let constructing = true;
  const index = stats().created;
  const watcher = facade.createWatcher('controlled.valid', defs, (update) => {
    check(!constructing, 'Native initial callback is asynchronous');
    updates.push(update);
  });
  constructing = false;
  await turn();
  check(
    !updates.length && stats().polled > 0,
    'No completed snapshot is not an observed empty policy'
  );
  native.feed(index, 0);
  await turn();
  check(updates.length === 1 && !Object.keys(updates[0]).length, 'Actual completed empty snapshot');
  native.feed(index, 1);
  await turn();
  check(
    updates.at(-1).Text === 'managed Grüße 日本語' &&
      updates.at(-1).Count === 7.5 &&
      updates.at(-1).Mixed === 'union',
    'Genuine native JSON typed snapshot'
  );
  native.feed(index, 2);
  await turn();
  check(
    updates.at(-1).Text === undefined && updates.at(-1).Count === 8,
    'Facade removal translation from actual native full snapshot'
  );
  watcher.dispose();
  native.feed(index, 1);
  await turn();
  check(updates.length === 3, 'Disposed provider cannot deliver stale feed');
  check(
    native.stop(99999) === false &&
      native.stop(-1) === false &&
      native.stop('1') === false &&
      native.stop(NaN) === false,
    'Unknown/invalid IDs cannot target another watcher'
  );

  function installCollectable() {
    const marker = { value: 'actual native closure root' };
    let resolve;
    const done = new Promise((finish) => {
      resolve = finish;
    });
    let id;

    const callback = (json) => {
      check(
        marker.value === 'actual native closure root' && JSON.parse(json).values.Count === 8,
        'Native callback root survived real GC'
      );
      check(native.stop(id) && !native.stop(id), 'Reentrant native stop exactly once');
      native.collect();
      check(marker.value === 'actual native closure root', 'Active frame survives stop plus GC');
      resolve();
    };

    id = native.start('controlled.gc', wire, callback);
    return { id, weak: new WeakRef(callback), done, index: stats().created - 1 };
  }

  const gc = installCollectable();
  await turn();
  await native.collectAtIdle();

  function inspect() {
    check(typeof gc.weak.deref() === 'function', 'Actual native active callback root');
  }

  inspect();
  await turn();
  native.feed(gc.index, 2);
  await gc.done;
  for (let i = 0; i < 12; i++) {
    await turn();
    await native.collectAtIdle();
  }
  check(gc.weak.deref() === undefined, 'Native stop releases callback root after active frame');

  const ids = Array.from({ length: 16 }, () => native.start('controlled.quota', '{}', () => {}));

  fails(() => native.start('controlled.quota', '{}', () => {}), 'ERR_MITE_POLICY_LIMIT');

  ids.forEach((id) => check(native.stop(id), 'Every quota owner stops'));

  fails(() => native.start('controlled.quota', '{}', () => {}), 'ERR_MITE_POLICY_LIMIT');

  await turn();
  const replacement = native.start('controlled.after-drain', '{}', () => {});
  check(
    replacement > ids.at(-1) && native.stop(replacement),
    'Closing timers count until real UV drain and IDs never reuse'
  );
  await turn();
  check(
    stats().created === stats().destroyed && stats().stopped === stats().created,
    'All providers stopped and destroyed before VM teardown'
  );
  clearTimeout(watchdog);
  console.log(
    'PASS production policy Host native owner timer async snapshots validation GC reentry stale IDs closing quota and disposal'
  );
}
