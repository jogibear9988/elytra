/** Run with NODE_DEBUG=mix*,SELECT; writes only fixed, owned diagnostic text. */
import assert from 'node:assert/strict';
import { debuglog, debug } from 'node:util';
assert.equal(debug, debuglog);
let callbacks = 0;
const enabled = debuglog('MiXeD', (logger) => {
  callbacks++;
  assert.equal(logger.enabled, true);
});
const disabled = debuglog('other');
assert.equal(enabled.enabled, true);
assert.equal(disabled.enabled, false);
enabled('value %d', 7);
disabled('must not be written');
assert.equal(callbacks, 1);
console.log('PASS genuine NODE_DEBUG matching and stderr logger');
