/** Preserve bounded Mite pressure versus Node's early native write acceptance. */
import { readFileSync } from 'node:fs';
import { setTimeout as sleep } from 'node:timers/promises';
const fixtures = JSON.parse(readFileSync(process.argv[2], 'utf8'));
const decoder = new DecompressionStream('gzip');
const writer = decoder.writable.getWriter();
let written = false;
const writing = writer.write(Buffer.from(fixtures.gzip, 'hex')).then(
  () => {
    written = true;
  },
  (error) => error
);
await sleep(5);
console.log('write accepted without reads', written);
const reason = new Error('cancel inflate');
await decoder.readable.cancel(reason);
console.log('pending write cancelled', (await writing) === reason);
try {
  await writer.closed;
  throw new Error('missing closed rejection');
} catch (error) {
  if (error !== reason) {
    throw error;
  }
}
console.log('DONE inflate pressure');
