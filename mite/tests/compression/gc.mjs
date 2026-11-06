/** Force real VM collections between native inflate steps using the non-GUI host. */
import { readFileSync } from 'node:fs';
const fixture = JSON.parse(readFileSync(process.argv[2], 'utf8'));
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
for (let iteration = 0; iteration < 4; iteration++) {
  const source = ReadableStream.from([Buffer.from(fixture.gzip, 'hex')]);
  const reader = source.pipeThrough(new DecompressionStream('gzip')).getReader();
  let length = 0;
  while (true) {
    collect();
    const { value, done } = await reader.read();
    if (done) {
      break;
    }
    length += value.byteLength;
  }
  if (length !== fixture.text) {
    throw new Error('GC lost inflater state');
  }
  reader.releaseLock();
  collect();
}
console.log('PASS forced-GC native inflater lifetime');
