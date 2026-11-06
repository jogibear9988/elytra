/** Consume compressed file bytes through the standard Web Stream interface. */
import { readFileSync } from 'node:fs';
if (!process.argv[2]) {
  throw new Error('Usage: mite decompress.mjs <file> [gzip|deflate|deflate-raw]');
}
const source = ReadableStream.from([readFileSync(process.argv[2])]);
const decoded = source.pipeThrough(new DecompressionStream(process.argv[3] ?? 'gzip'));
let bytes = 0;
for await (const chunk of decoded) {
  bytes += chunk.byteLength;
}
console.log('bytes', bytes);
