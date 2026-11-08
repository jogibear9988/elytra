/** Mite-only actual native admission bound, close reservation and shutdown drain. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
fs.writeFileSync('capacity.txt', 'unchanged');
const options = { persistent: false };

const callback = () => {};

const opened = [];
for (let index = 0; index < 256; index++) {
  opened.push(fs.watch('capacity.txt', options, callback));
}

assert.throws(() => fs.watch('capacity.txt', options, callback), { code: 'ENOSPC' });

for (const watcher of opened) {
  watcher.close();
}

assert.throws(() => fs.watch('capacity.txt', options, callback), { code: 'ENOSPC' });

const deadline = Date.now() + 5000;
for (let index = 0; index < 256; index++) {
  while (true) {
    try {
      fs.watch('capacity.txt', options, callback);
      break;
    } catch (error) {
      assert.equal(error.code, 'ENOSPC');
      assert.ok(Date.now() < deadline, 'True native close completion releases admission');
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  }
}

assert.throws(() => fs.watch('capacity.txt', options, callback), { code: 'ENOSPC' });
// All 256 real native handles remain unreferenced at natural runtime shutdown.
console.log(
  'PASS actual event watcher 256 active/closing cap, native-drain recovery and VM shutdown'
);
