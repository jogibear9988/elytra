/** Exercise actual LibJS GC inside the native CJS analyzer/cache/snapshot path. */
import { createRequire } from 'node:module';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

check(typeof collectHostGarbage === 'function', 'Requires actual embedded LibJS collector');
const require = createRequire(import.meta.url);

/** A completed helper frame leaves no application strong reference to original. */
function replaceOriginal() {
  const filename = require.resolve('./original.cjs');
  const original = require(filename);
  const weak = new WeakRef(original);
  require.cache[filename].exports = { named: 'replacement' };
  return weak;
}

const weakOriginal = replaceOriginal();
for (let turn = 0; turn < 4; ++turn) {
  await new Promise((resolve) => setImmediate(resolve));
  collectHostGarbage();
}
const originalNamespace = await import('./original.cjs');
check(
  originalNamespace.default === weakOriginal.deref(),
  'Private original-exports cache survives collection'
);
check(
  originalNamespace.default !== require('./original.cjs'),
  'Require replacement remains distinct'
);
check(originalNamespace.named === 'original', 'Original named value retained before first import');
check(
  originalNamespace.payload.call() === 'retained' &&
    originalNamespace.payload.nested.join(',') === '1,2,3',
  'Original nested data and closure retained'
);
check(
  originalNamespace['module.exports'] === originalNamespace.default,
  'Canonical alias identity'
);
console.log('PASS native CJS original exports survive pre-import collection');

// The first import initialized the private parser. Nothing in application code
// holds that function; collect before parsing either new CommonJS source.
for (let turn = 0; turn < 4; ++turn) {
  await new Promise((resolve) => setImmediate(resolve));
  collectHostGarbage();
}
globalThis.snapshotFirstCalls = 0;
globalThis.snapshotSecondCalls = 0;
const snapshot = await import('./snapshot.cjs');
check(
  snapshot.first.marker === 'ephemeral',
  'First ephemeral getter object survives later getter GC'
);
check(
  snapshot.first.nested.bytes.join(',') === '0,255,37',
  'Nested binary survives snapshot collection'
);
check(snapshot.second === 'collected', 'Second getter executed actual collector');
check(
  globalThis.snapshotFirstCalls === 1 && globalThis.snapshotSecondCalls === 1,
  'Getter snapshot runs once'
);
check(snapshot === (await import('./snapshot.cjs')), 'Post-collection namespace cache identity');
check(
  globalThis.snapshotFirstCalls === 1 && globalThis.snapshotSecondCalls === 1,
  'Cached import does not repeat getters'
);
console.log('PASS native CJS snapshot roots survive getter-triggered collection');

collectHostGarbage();
const fresh = await import('./fresh.cjs');
check(
  fresh.afterCollection === 42 && fresh.default.afterCollection === 42,
  'Private parser remains callable after collection'
);
check(fresh === (await import('./fresh.cjs')), 'Fresh analyzed namespace cache identity');
console.log('PASS private CJS parser and namespace cache survive collection');
