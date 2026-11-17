/** Real pipe pressure: the independent parent drains slowly; no fs-write substitution. */
import assert from 'node:assert/strict';
const callbacks = [];
let drains = 0;
let backpressured = false;

process.stdout.on('drain', () => drains++);

const completed = [];
for (let piece = 0; piece < 32; piece++) {
  const length = 65536 + piece * 13;
  const storage = Buffer.alloc(length + 7, 233);
  const bytes = storage.subarray(4, length + 4);
  for (let offset = 0; offset < length; offset++) {
    bytes[offset] = (offset * 17 + piece) & 255;
  }
  completed.push(
    new Promise((resolve, reject) => {
      const accepted = process.stdout.write(bytes, (error) => {
        if (error) {
          return reject(error);
        }
        callbacks.push(piece);
        // A successful callback releases the writer's borrow, not earlier.
        storage.fill(211);
        resolve();
      });
      if (!accepted) {
        backpressured = true;
      }
    })
  );
}
await Promise.all(completed);
await new Promise((done) => setImmediate(done));
assert.deepEqual(
  callbacks,
  Array.from({ length: 32 }, (_, index) => index)
);
assert.equal(backpressured, true);
assert.ok(drains > 0);
process.stderr.write('PASS native pipe backpressure callbacks/FIFO/drain\n');
