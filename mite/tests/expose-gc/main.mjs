/** Real opt-in collection: live data survives and a discarded weak target is reclaimed. */
const enabled = process.execArgv.includes('--expose-gc');
const mite = typeof process.versions.mite === 'string';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

check(typeof globalThis.gc === (enabled ? 'function' : 'undefined'), 'GC opt-in boundary');
if (enabled) {
  const descriptor = Object.getOwnPropertyDescriptor(globalThis, 'gc');
  check(
    gc.name === 'gc' &&
      gc.length === 0 &&
      descriptor.writable &&
      descriptor.enumerable &&
      !descriptor.configurable,
    'native gc function/global descriptor'
  );
  const held = { bytes: new Uint8Array([0, 128, 255]) };
  held.self = held;
  const weak = (() => new WeakRef({ marker: 'weak target' }))();
  check(weak.deref()?.marker === 'weak target', 'weak target initially alive');
  check(
    gc() === undefined && gc(undefined) === undefined && Reflect.apply(gc, {}, []) === undefined,
    'synchronous GC return and receiver'
  );
  check(
    held.self === held &&
      held.bytes.join() === '0,128,255' &&
      weak.deref()?.marker === 'weak target',
    'GC must preserve live roots and weak targets kept during this job'
  );

  function alive() {
    return weak.deref() !== undefined;
  }

  let reclaimed = false;
  for (let attempt = 0; attempt < 100; ++attempt) {
    await new Promise((resolve) => setTimeout(resolve, 1));
    gc();
    if (!alive()) {
      reclaimed = true;
      break;
    }
  }
  check(
    reclaimed && held.self === held && held.bytes[2] === 255,
    'real GC must reclaim discarded target while retaining roots'
  );
  if (mite) {
    let gets = 0;
    const options = new Proxy(
      {},
      {
        get() {
          ++gets;
          throw Error('option getter');
        },

        ownKeys() {
          ++gets;
          throw Error('option keys');
        }
      }
    );
    for (const args of [
      [null],
      [false],
      [1],
      [{}],
      [{ type: 'major' }],
      [{ type: 'minor' }],
      [{ execution: 'sync' }],
      [{ execution: 'async' }],
      [{ flavor: 'last-resort' }],
      [options],
      [undefined, {}]
    ]) {
      let code;
      try {
        gc(...args);
      } catch (error) {
        code = error.code;
      }
      check(code === 'ERR_MITE_UNSUPPORTED_API', 'unsupported GC mode must fail explicitly');
    }
    check(gets === 0, 'unsupported GC options must not invoke getters/traps');
  }
}
console.log(
  JSON.stringify({
    enabled,
    argv: process.argv.slice(2),
    flags: process.execArgv,
    collected: enabled
  })
);
