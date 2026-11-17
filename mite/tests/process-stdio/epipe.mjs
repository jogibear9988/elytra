/** Parent closes its only read end before releasing this real stdin gate. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
const gate = Buffer.alloc(1);
assert.equal(fs.readSync(0, gate, 0, 1, null), 1);
assert.equal(gate[0], 103);
let callbacks = 0;
let events = 0;
let callbackError;
let emittedError;
const failedEvent = new Promise((done) => {
  process.stdout.once('error', (error) => {
    events++;
    emittedError = error;
    done();
  });
});
const failedCallback = new Promise((done) => {
  process.stdout.write('owned closed pipe\n', (error) => {
    callbacks++;
    callbackError = error;
    done();
  });
});
await Promise.all([failedEvent, failedCallback]);
assert.equal(callbacks, 1);
assert.equal(events, 1);
assert.equal(callbackError.code, 'EPIPE');
assert.equal(emittedError.code, 'EPIPE');
assert.equal(callbackError, emittedError);
process.stderr.write('PASS native closed pipe EPIPE callback/error identity\n');
