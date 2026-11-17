/** Deliberate standalone loader boundaries, not Node data-module parity claims. */
import assert from 'node:assert/strict';
import { createRequire, registerHooks } from 'node:module';

assert.ok(process.versions.mite);
for (const url of [
  'mite:internal/cjs',
  'mite:internal/builtin',
  'mite:internal/host',
  'mite-host:missing'
]) {
  const hook = registerHooks({ resolve: () => ({ url, shortCircuit: true }) });
  try {
    await assert.rejects(import('owned-private-data-hook'), {
      code: 'ERR_UNSUPPORTED_ESM_URL_SCHEME'
    });
  } finally {
    hook.deregister();
  }
}
const require = createRequire(import.meta.url);

assert.throws(() => require('data:text/javascript,export default 1'), {
  code: 'ERR_MITE_UNSUPPORTED_REQUIRE_ESM'
});

await assert.rejects(import('data:application/wasm;base64,AGFzbQEAAAA='), {
  code: 'ERR_MITE_UNSUPPORTED_API'
});
for (const mime of ['APPLICATION/WASM', ' application/wasm', 'application/wasm ']) {
  await assert.rejects(import('data:' + mime + ';base64,AGFzbQEAAAA='), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT'
  });
}
console.log('PASS data module private bridges and unsupported synchronous/wasm loading');

// Valid JavaScript comments exceed each independent admission bound. Rejection
// must happen before source compilation and must not poison a later small load.
await assert.rejects(import('data:text/javascript,//' + 'x'.repeat(4 * 1024 * 1024)), {
  code: 'ERR_MITE_DATA_MODULE_LIMIT'
});
await assert.rejects(import('data:text/javascript,//' + 'x'.repeat(8 * 1024 * 1024)), {
  code: 'ERR_MITE_DATA_MODULE_LIMIT'
});
assert.equal((await import('data:text/javascript,export default 42')).default, 42);
console.log('PASS data module URL/source quotas reject with recovery');

// Distinct fragment identities share a modest comment body, avoiding a large
// executable AST. Current accounting reserves twice URL plus twice source bytes.
const comment =
  'data:text/javascript,export%20const%20marker%3D%7B%7D%3B/*' + 'x'.repeat(768 * 1024) + '*/';
const admitted = [];
let quota;
for (let index = 0; index < 16; index++) {
  const url = comment + '#owned-' + index;
  try {
    const module = await import(url);
    // Repeated imports exercise the native cache without another reservation.
    for (let repeat = 0; repeat < 3; repeat++) {
      assert.equal(await import(url), module);
    }
    admitted.push({ url, module });
  } catch (error) {
    quota = error;
    break;
  }
}
assert.equal(quota?.code, 'ERR_MITE_DATA_MODULE_LIMIT');
assert.equal(admitted.length, 10, 'Cache hits must not consume another native reservation');
const decodedBytes = 768 * 1024 + 'export const marker={};/**/'.length;
const charged = admitted.reduce((sum, entry) => sum + 2 * entry.url.length + 2 * decodedBytes, 0);
assert.ok(charged <= 32 * 1024 * 1024);
assert.ok(charged + 2 * (comment + '#owned-10').length + 2 * decodedBytes > 32 * 1024 * 1024);
for (const entry of admitted) {
  assert.equal(await import(entry.url), entry.module);
  assert.equal((await import(entry.url)).marker, entry.module.marker);
}
console.log('PASS data module aggregate quota/cache hit ownership and recovery');
