import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

assert.equal(typeof globalThis.gc, 'function', 'Run with actual native GC enabled');
const require = createRequire(import.meta.url);
assert.equal(require('module'), Module);
assert.equal(require('node:module'), Module);
assert.equal(Module.Module, Module);
assert.equal(Module._cache, require.cache);
const originalLoad = Module._load;
const originalResolve = Module._resolveFilename;
const originalLookup = Module._resolveLookupPaths;
const calls = [];
const virtual = { marker: {} };
const failure = Error('owned-interceptor-failure');
const receiver = {};
let capturedParent;
let innerCalls = 0;

Module._load = function inner(request, parent, isMain) {
  innerCalls++;
  return originalLoad.apply(this, arguments);
};

const previous = Module._load;

Module._load = function outer(request, parent, isMain) {
  assert.equal(this, Module);
  assert.ok(parent instanceof Module);
  assert.equal(typeof parent.filename, 'string');
  assert.equal(isMain, false);
  calls.push({ request, parent });
  capturedParent = parent;
  if (request === 'owned-virtual') {
    return virtual;
  }
  if (request === 'owned-throw') {
    globalThis.gc();
    throw failure;
  }
  return previous.apply(this, arguments);
};

try {
  assert.equal(require('owned-virtual'), virtual);
  assert.equal(require('owned-virtual'), virtual);
  assert.equal(calls.length, 2);
  assert.equal(innerCalls, 0, 'Virtual interception precedes resolution');
  assert.equal(capturedParent.filename, fileURLToPath(import.meta.url));
  assert.equal(require('fs'), require('node:fs'));
  assert.equal(innerCalls, 2, 'Both real wrappers delegate builtins');

  assert.throws(
    () => require('owned-throw'),
    (error) => error === failure
  );

  assert.equal(originalLoad.call(receiver, 'node:fs', capturedParent, false), require('node:fs'));

  assert.throws(() => originalLoad.call(receiver, 'owned-virtual', capturedParent, false), {
    code: 'MODULE_NOT_FOUND'
  });

  console.log('PASS live Module load wrappers preserve interception/receiver/parent/fallback');

  const leaf = require('./leaf.cjs');
  const leafName = require.resolve('./leaf.cjs');
  const before = calls.filter((entry) => entry.request === './leaf.cjs').length;
  assert.equal(require('./leaf.cjs'), leaf);
  assert.equal(calls.filter((entry) => entry.request === './leaf.cjs').length, before + 1);
  assert.equal(leaf.loads, 1);
  assert.equal(require.cache[leafName].exports, leaf);
  const nested = require('./nested.cjs');
  assert.equal(nested.leaf, leaf);
  assert.equal(nested.builtin, require('node:fs'));
  const nestedLeaf = calls.find(
    (entry) => entry.request === './leaf.cjs' && entry.parent === nested.parent
  );
  assert.ok(nestedLeaf);
  assert.equal(nestedLeaf.parent.filename, require.resolve('./nested.cjs'));
  const cycle = require('./cycle-a.cjs');
  assert.equal(cycle.b.a, cycle);
  assert.equal(cycle.b.observedPhase, 'opening');
  assert.equal(cycle.phase, 'ready');
  console.log('PASS Module interception retains real cache/nested/circular behavior');

  globalThis.__ownedLoadFailure = failure;
  const retryName = require.resolve('./retry.cjs');

  assert.throws(
    () => require('./retry.cjs'),
    (error) => error === failure
  );

  assert.equal(require.cache[retryName], undefined);
  globalThis.gc();
  const retried = require('./retry.cjs');
  assert.equal(retried.loads, 2);
  assert.equal(require('./retry.cjs'), retried);
  console.log('PASS failed CommonJS load rolls back cache and preserves exception through GC');

  // A captured original load still uses the current real resolver. No hooks
  // are faked in the fixture's own require function.
  let resolveSeen = false;

  Module._resolveFilename = function resolve(request, parent, isMain, options) {
    if (request === 'node:fs') {
      throw Error('Prefixed builtin loads must bypass the public filename resolver');
    }
    if (request === 'owned-resolved') {
      assert.equal(parent, capturedParent);
      resolveSeen = true;
      return leafName;
    }
    return originalResolve.apply(this, arguments);
  };

  globalThis.gc();
  assert.equal(originalLoad.call(receiver, 'owned-resolved', capturedParent, false), leaf);
  assert.equal(resolveSeen, true);
  assert.equal(originalLoad.call(receiver, 'node:fs', capturedParent, false), require('fs'));
  console.log('PASS captured original load honors live resolver after GC');

  const external = fileURLToPath(new URL('./external/', import.meta.url));
  const paths = [external];
  const options = { paths };

  assert.throws(() => require.resolve('owned-alternate'), { code: 'MODULE_NOT_FOUND' });

  const resolved = require.resolve('owned-alternate', options);
  assert.equal(require(resolved), 'external-package');
  assert.equal(options.paths, paths);
  assert.deepEqual(paths, [external]);
  const lookup = Module._nodeModulePaths(external);
  assert.ok(lookup.includes(fileURLToPath(new URL('./external/node_modules', import.meta.url))));
  globalThis.__ownedLookupPaths = lookup;
  const parentPaths = require('./parent-paths.cjs');
  assert.equal(parentPaths.value, 'external-package');
  assert.deepEqual(parentPaths.paths, lookup);

  Module._resolveLookupPaths = function lookupPaths(request, parent) {
    if (request === 'owned-alternate') {
      return lookup;
    }
    return originalLookup.apply(this, arguments);
  };
  // The original require from this parent previously failed. A live lookup
  // override must change actual resolution, not merely expose decorative paths.
  assert.equal(require('owned-alternate'), 'external-package');
  console.log('PASS actual options.paths and parent.paths govern package lookup');
} finally {
  Module._load = originalLoad;
  Module._resolveFilename = originalResolve;
  Module._resolveLookupPaths = originalLookup;
  delete globalThis.__ownedLoadFailure;
  delete globalThis.__ownedLookupPaths;
}
