/** Differential streaming/flush acceptance; external peers additionally verify wire formats. */
import * as zlib from 'node:zlib';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

const check = (condition, message) => {
  if (!condition) {
    throw Error(message);
  }
};

check(require('zlib').createDeflateRaw === zlib.createDeflateRaw, 'CJS/ESM identity');
const payload = Buffer.from('héllo\0😀'.repeat(32000));

const collect = (stream) =>
  new Promise((resolve, reject) => {
    const chunks = [];

    stream.on('data', (chunk) => chunks.push(chunk));

    stream.once('error', reject);

    stream.once('end', () => resolve(Buffer.concat(chunks)));
  });

for (const [compress, expand] of [
  ['DeflateRaw', 'InflateRaw'],
  ['Deflate', 'Inflate'],
  ['Gzip', 'Gunzip']
]) {
  const compressed = zlib[compress[0].toLowerCase() + compress.slice(1) + 'Sync'](payload, {
    chunkSize: 64
  });
  const decoded = zlib[expand[0].toLowerCase() + expand.slice(1) + 'Sync'](compressed, {
    chunkSize: 64
  });
  check(decoded.equals(payload), compress + ' sync');
  const encoder = zlib['create' + compress]({ chunkSize: 64, readableHighWaterMark: 128 });
  const wire = collect(encoder);
  encoder.write(payload.subarray(0, 3));
  encoder.write(payload.subarray(3, 1000));
  encoder.end(payload.subarray(1000));
  const decoder = zlib['create' + expand]({ chunkSize: 64 });
  const result = collect(decoder);
  const bytes = await wire;
  for (let i = 0; i < bytes.length; i += 13) {
    decoder.write(bytes.subarray(i, i + 13));
  }
  decoder.end();
  check((await result).equals(payload), compress + ' fragmented stream');
  const asyncResult = await new Promise((resolve, reject) =>
    zlib[compress[0].toLowerCase() + compress.slice(1)](payload, (e, r) =>
      e ? reject(e) : resolve(r)
    )
  );
  check(
    zlib[expand[0].toLowerCase() + expand.slice(1) + 'Sync'](asyncResult).equals(payload),
    'async ' + compress
  );
}
// RFC7692-style context takeover: remove/reinsert the sync-flush trailer per message.
const deflater = zlib.createDeflateRaw({ windowBits: 15 });
const inflater = zlib.createInflateRaw({ windowBits: 15 });
let encoded = [];
let decoded = [];

deflater.on('data', (c) => encoded.push(c));

inflater.on('data', (c) => decoded.push(c));

for (const text of ['abc'.repeat(4000), 'abc'.repeat(7000), 'Unicode ä 😀'.repeat(300)]) {
  deflater.write(Buffer.from(text));
  await new Promise((resolve, reject) => {
    deflater.once('error', reject);
    deflater.flush(zlib.constants.Z_SYNC_FLUSH, resolve);
  });
  const wire = Buffer.concat(encoded);
  encoded = [];
  check(wire.subarray(-4).equals(Buffer.from([0, 0, 255, 255])), 'sync flush trailer');
  inflater.write(wire.subarray(0, -4));
  inflater.write(Buffer.from([0, 0, 255, 255]));
  await new Promise((resolve, reject) => {
    inflater.once('error', reject);
    inflater.flush(resolve);
  });
  check(Buffer.concat(decoded).toString() === text, 'context takeover');
  decoded = [];
}
deflater.close();
inflater.close();
const gzip = zlib.gzipSync('one');
check(
  zlib.gunzipSync(Buffer.concat([gzip, zlib.gzipSync('two'), Buffer.alloc(8)])).toString() ===
    'onetwo',
  'gzip members/padding'
);
for (const [bytes, expected] of [
  [Buffer.from([1, 2, 3]), 'Z_DATA_ERROR'],
  [gzip.subarray(0, -2), 'Z_BUF_ERROR']
]) {
  let error;
  try {
    zlib.gunzipSync(bytes);
  } catch (e) {
    error = e;
  }
  check(error?.code === expected, 'corrupt/truncated ' + expected);
}
let tooLarge;
try {
  zlib.gunzipSync(zlib.gzipSync(payload), { maxOutputLength: 8 });
} catch (e) {
  tooLarge = e;
}
check(tooLarge?.code === 'ERR_BUFFER_TOO_LARGE', 'output limit');
console.log(
  'PASS zlib sync/async/stream formats, fragments, context takeover, members/checksum/truncation and output cap'
);
const badCRC = Buffer.from(gzip);
badCRC[badCRC.length - 8] ^= 128;
let checksum;
try {
  zlib.gunzipSync(badCRC);
} catch (error) {
  checksum = error;
}
check(checksum?.code === 'Z_DATA_ERROR', 'gzip checksum refusal');
let callbacks = 0;
await new Promise((resolve) =>
  zlib.gunzip(zlib.gzipSync(payload), { maxOutputLength: 8 }, (error) => {
    callbacks++;
    check(error?.code === 'ERR_BUFFER_TOO_LARGE', 'async output cap');
    resolve();
  })
);
check(callbacks === 1, 'async failure callback once');
for (const [encode, decode] of [
  ['gzipSync', 'gunzipSync'],
  ['deflateSync', 'inflateSync'],
  ['deflateRawSync', 'inflateRawSync']
]) {
  check(zlib[decode](zlib[encode](Buffer.alloc(0))).length === 0, 'empty ' + encode);
}
