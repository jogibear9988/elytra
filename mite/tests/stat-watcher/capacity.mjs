/** Actual Mite admission/teardown test. Node intentionally has no corresponding 256-handle quota. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const prefix = path.join(os.tmpdir(), `elytra-statwatch-capacity-${process.pid}-`);
const options = { persistent: false, interval: 10_000 };

const listener = () => {};

const watched = Array.from({ length: 256 }, (_, index) => `${prefix}initial-${index}`);
for (const file of watched) {
  fs.watchFile(file, options, listener);
}

assert.throws(() => fs.watchFile(`${prefix}overflow`, options, listener), { code: 'ENOSPC' });

for (const file of watched) {
  fs.unwatchFile(file);
}
// No native loop turn has run: the old worker-stat/uv_close handles still own
// their quota. Retiring JS wrappers cannot create an unbounded native close queue.
assert.throws(() => fs.watchFile(`${prefix}closing-overflow`, options, listener), {
  code: 'ENOSPC'
});

const deadline = Date.now() + 5_000;
for (let index = 0; index < 256; ++index) {
  while (true) {
    try {
      fs.watchFile(`${prefix}shutdown-${index}`, options, listener);
      break;
    } catch (error) {
      assert.equal(error.code, 'ENOSPC');
      assert.ok(Date.now() < deadline, 'native close completion must eventually release admission');
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  }
}

assert.throws(() => fs.watchFile(`${prefix}final-overflow`, options, listener), { code: 'ENOSPC' });
// Leave all 256 real native watchers active but unreferenced. Successful process
// exit verifies EventLoop teardown retires callback roots and drains in-flight
// polling/close state without requiring application JS to release every wrapper.
console.log(
  'PASS actual stat watcher 256 active/closing quota, native-drain recovery and 256 unreferenced watchers at VM shutdown'
);
