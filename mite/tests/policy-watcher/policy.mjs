/** Actual facade, controlled native policy snapshots; no managed preference writes or OS acceptance. */
import create from '../../modules/system/policy-watcher.mjs';

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
  return caught;
}

const tick = () => new Promise((resolve) => queueMicrotask(resolve));

function fixture() {
  const state = { starts: [], stops: [], callbacks: [], next: 0 };
  const native = {
    start(product, definitions, callback) {
      state.starts.push({ product, definitions: JSON.parse(definitions) });
      state.callbacks.push(callback);
      state.onStart?.(callback);
      if (state.failure) {
        throw state.failure;
      }
      return Object.hasOwn(state, 'result') ? state.result : ++state.next;
    },

    stop(id) {
      state.stops.push(id);
      state.onStop?.();
      return true;
    }
  };
  const api = create(native);
  return {
    api,
    state,
    native,

    emit: (values, index = 0) => state.callbacks[index](JSON.stringify({ values }))
  };
}

const defs = {
  Text: { type: 'string' },
  Count: { type: 'number' },
  Enabled: { type: 'boolean' },
  Mixed: { type: ['string', 'boolean', 'number'] }
};

{
  const f = fixture();
  const updates = [];
  const watcher = f.api.createWatcher('org.elytra.unused', defs, (value) => updates.push(value));
  check(updates.length === 0, 'No fabricated empty initial snapshot');

  f.emit({});
  check(updates.length === 0, 'Initial callback asynchronous');
  await tick();
  check(
    updates.length === 1 && !Object.keys(updates[0]).length,
    'Actual empty snapshot delivered once'
  );
  f.emit({ Text: 'managed', Count: 3, Enabled: false, Mixed: true });
  await tick();
  check(
    updates[1].Text === 'managed' && updates[1].Count === 3 && updates[1].Enabled === false,
    'Scalar snapshot'
  );
  f.emit({ Text: 'managed', Count: 3, Enabled: false, Mixed: true });
  await tick();
  check(updates.length === 2, 'Identical snapshot suppressed');
  f.emit({ Text: 'managed', Count: 4, Mixed: 'union' });
  await tick();
  check(
    updates[2].Text === 'managed' &&
      updates[2].Count === 4 &&
      Object.hasOwn(updates[2], 'Enabled') &&
      updates[2].Enabled === undefined,
    'Changed snapshot includes unchanged present values and undefined removal'
  );
  watcher.dispose();
  watcher.dispose();
  check(f.state.stops.length === 1, 'Idempotent native disposal');
  const scheduler = fixture();
  let delivered = 0;
  const schedulerWatcher = scheduler.api.createWatcher('org.elytra', {}, () => {
    delivered++;
  });
  const originalQueue = globalThis.queueMicrotask;
  try {
    globalThis.queueMicrotask = () => {
      throw Error('Mutable global scheduler reached');
    };

    scheduler.emit({});
  } finally {
    globalThis.queueMicrotask = originalQueue;
  }
  await tick();
  check(delivered === 1, 'Factory captured scheduler remains private');
  schedulerWatcher.dispose();

  console.log(
    'PASS genuine initial snapshot asynchronous delivery unchanged suppression and full changed payload removals'
  );
}

{
  const f = fixture();
  const source = Object.create(null);
  source.__proto__ = {
    type: ['string', 'string', 'boolean'],
    description: 'CodeOSS metadata ignored as upstream'
  };
  source.constructor = { type: 'string' };
  const updates = [];
  const watcher = f.api.createWatcher('org.elytra.unused', source, (update) =>
    updates.push(update)
  );
  source.__proto__.type.push('number');
  check(
    f.state.starts[0].definitions.__proto__.type.length === 2 &&
      !Object.hasOwn(f.state.starts[0].definitions.__proto__, 'description'),
    'Owned projected type snapshot'
  );
  const values = JSON.parse('{"__proto__":"safe","constructor":"value"}');
  f.emit(values);
  await tick();
  check(
    Object.getPrototypeOf(updates[0]) === null && updates[0].__proto__ === 'safe',
    'Safe null-prototype policy names'
  );
  updates[0].constructor = 'caller mutation';
  f.emit(values);
  await tick();
  check(updates.length === 1, 'Caller cannot mutate comparison state');
  watcher.dispose();
  console.log(
    'PASS union definition snapshots upstream metadata projection safe names and callback mutation isolation'
  );
}

{
  const f = fixture();
  for (const definition of [
    { type: [] },
    { type: ['string', 'object'] },
    { type: 'object' },
    null
  ]) {
    fails(
      () => f.api.createWatcher('org.elytra', { Value: definition }, () => {}),
      definition === null ? 'ERR_INVALID_ARG_TYPE' : 'ERR_INVALID_ARG_VALUE'
    );
  }

  fails(() => f.api.createWatcher('', {}, () => {}), 'ERR_INVALID_ARG_VALUE');

  fails(() => f.api.createWatcher('bad\0product', {}, () => {}), 'ERR_INVALID_ARG_VALUE');

  fails(() => f.api.createWatcher('bad\ud800', {}, () => {}), 'ERR_INVALID_ARG_VALUE');

  fails(() => f.api.createWatcher('org.elytra', {}, 1), 'ERR_INVALID_ARG_TYPE');

  fails(
    () => f.api.createWatcher('org.elytra', {}, () => {}, { registryPath: 'custom' }),
    'ERR_MITE_UNSUPPORTED_API'
  );

  fails(
    () => f.api.createWatcher('org.elytra', {}, () => {}, { unknown: false }),
    'ERR_MITE_UNSUPPORTED_API'
  );

  fails(
    () =>
      f.api.createWatcher(
        'org.elytra',
        Object.fromEntries(Array.from({ length: 257 }, (_, i) => ['v' + i, { type: 'boolean' }])),
        () => {}
      ),
    'ERR_MITE_POLICY_LIMIT'
  );

  const overLimit = Array(257);
  Object.defineProperty(overLimit, 0, {
    get() {
      throw Error('Over-limit member must not be read');
    }
  });

  fails(
    () => f.api.createWatcher('org.elytra', { Value: { type: overLimit } }, () => {}),
    'ERR_INVALID_ARG_VALUE'
  );

  check(!f.state.starts.length, 'Invalid input cannot start native observer');
  const valid = ['string', 'boolean', 'string'];

  valid[Symbol.iterator] = () => {
    throw Error('Application union iterator must not run');
  };

  const bounded = f.api.createWatcher('org.elytra', { Value: { type: valid } }, () => {});
  check(
    JSON.stringify(f.state.starts.at(-1).definitions.Value.type) === '["string","boolean"]',
    'Bounded indexed union snapshot ignores hostile iterator and deduplicates'
  );
  bounded.dispose();

  f.api.createWatcher('org.elytra', {}, () => {}, { registryPath: '' }).dispose();
  console.log(
    'PASS strict bounded scalar definitions options unsupported registry path and no native admission on invalid input'
  );
}

{
  const f = fixture();
  let calls = 0;

  f.state.onStart = (callback) => callback('{"values":{"Text":"first"}}');

  const watcher = f.api.createWatcher('org.elytra', defs, () => {
    calls++;
  });
  check(!calls, 'Synchronous peer cannot deliver during admission');
  await tick();
  check(calls === 1, 'Queued initial admitted callback');
  f.emit({ Text: 'queued' });
  watcher.dispose();
  await tick();
  f.emit({ Text: 'stale' });
  await tick();
  check(calls === 1, 'Dispose revokes queued and late callbacks');
  const g = fixture();
  let failedCalls = 0;
  g.state.failure = Object.assign(Error('Provider unavailable'), {
    code: 'ERR_MITE_POLICY_PROVIDER'
  });

  g.state.onStart = (callback) => callback('{"values":{}}');

  check(
    fails(
      () =>
        g.api.createWatcher('org.elytra', {}, () => {
          failedCalls++;
        }),
      'ERR_MITE_POLICY_PROVIDER'
    ) === g.state.failure,
    'Genuine native error preserved'
  );
  await tick();
  check(!failedCalls, 'Failed admission never publishes policy data');
  console.log(
    'PASS synchronous peer admission gating failed starts disposal queued data and stale callbacks'
  );
}

{
  for (const json of [
    'null',
    '{}',
    '{',
    '{"values":{"Unknown":true}}',
    '{"values":{"Text":false}}',
    '{"values":{"Count":1e309}}'
  ]) {
    const f = fixture();

    f.api.createWatcher('org.elytra', defs, () => {});

    fails(() => f.state.callbacks[0](json), 'ERR_MITE_POLICY_DATA');

    check(f.state.stops.length === 1, 'Malformed native snapshot retires observer');
    f.state.callbacks[0](json);
  }
  const f = fixture();

  f.state.onStart = (callback) => callback('bad json');

  fails(() => f.api.createWatcher('org.elytra', {}, () => {}), 'ERR_MITE_POLICY_DATA');

  check(f.state.stops.length === 1, 'Malformed synchronous peer cleans returned ID');
  const g = fixture();
  g.state.result = '1';

  fails(() => g.api.createWatcher('org.elytra', {}, () => {}), 'ERR_MITE_POLICY_DATA');

  console.log(
    'PASS malformed provider snapshots type mismatches finite numbers and invalid native IDs'
  );
}

{
  const f = fixture();
  const watchers = Array.from({ length: 16 }, () =>
    f.api.createWatcher('org.elytra', {}, () => {})
  );

  fails(() => f.api.createWatcher('org.elytra', {}, () => {}), 'ERR_MITE_POLICY_LIMIT');

  watchers.forEach((watcher) => watcher.dispose());

  const g = fixture();
  let watcher;
  watcher = g.api.createWatcher('org.elytra', { Text: { type: 'string' } }, () =>
    watcher.dispose()
  );
  g.emit({ Text: 'first' });
  g.emit({ Text: 'second' });
  await tick();
  await tick();
  check(g.state.stops.length === 1, 'Callback reentry revokes remaining queued updates');
  const h = fixture();
  const owned = h.api.createWatcher('org.elytra', {}, () => {});

  h.state.onStop = () => h.emit({});

  owned.dispose();
  await tick();
  check(h.state.stops.length === 1, 'Native stop reentry cannot restore callback');
  console.log('PASS watcher quota callback reentry and native stop ownership revocation');
}
console.log('DONE policy watcher facade');
