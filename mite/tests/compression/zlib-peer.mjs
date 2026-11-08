/** Exchange real gzip/zlib/raw streams with an independent Python codec. */
import * as zlib from 'node:zlib';
import { readFileSync, writeFileSync } from 'node:fs';
const [directory] = process.argv.slice(2);
const payload = readFileSync(directory + '/payload');
for (const [kind, encode, decode] of [
  ['raw', 'deflateRawSync', 'inflateRawSync'],
  ['zlib', 'deflateSync', 'inflateSync'],
  ['gzip', 'gzipSync', 'gunzipSync']
]) {
  const foreign = readFileSync(directory + '/' + kind);
  if (!zlib[decode](foreign).equals(payload)) {
    throw Error('Foreign ' + kind);
  }
  writeFileSync(directory + '/out-' + kind, zlib[encode](payload));
}
console.log('PASS independent byte peers');
