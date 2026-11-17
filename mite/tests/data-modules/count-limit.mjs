/** A separate process isolates successful-record admission from byte-budget tests. */
import assert from 'node:assert/strict';

assert.ok(process.versions.mite);
// Neither decoding failures nor genuine JavaScript parse failures create a
// successful module record, even when many distinct identities are attempted.
for (let index = 0; index < 8; index++) {
  await assert.rejects(import('data:text/javascript;base64,%%%%#decode-' + index));
  await assert.rejects(import('data:text/javascript,export%20const%20%3D#parse-' + index));
}
const firstURL = 'data:text/javascript,export%20const%20marker%3D%7B%7D%3B#record-0';
const first = await import(firstURL);
for (let index = 1; index < 1024; index++) {
  // Re-reading the same real module cannot exhaust the independent count quota.
  assert.equal(await import(firstURL), first);
  await import('data:text/javascript,export%20default%20' + index + '#record-' + index);
}
await assert.rejects(import('data:text/javascript,export%20default%201025#overflow'), {
  code: 'ERR_MITE_DATA_MODULE_LIMIT'
});
assert.equal(await import(firstURL), first);
assert.equal((await import(firstURL)).marker, first.marker);
console.log('PASS data module successful-record quota excludes failures/cache hits');
