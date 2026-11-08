/** Mite's deliberately bounded/unsupported options fail explicitly. */
import * as zlib from 'node:zlib';

function refuses(callback, code) {
  try {
    callback();
  } catch (error) {
    if (error.code === code) {
      return;
    }
    throw error;
  }
  throw Error('Missing ' + code);
}

for (const options of [
  { dictionary: Buffer.from('key') },
  { info: true },
  { emitClose: false },
  { objectMode: true },
  { decodeStrings: false },
  { flush: 1 },
  { flush: 5 }
]) {
  refuses(() => zlib.createInflateRaw(options), 'ERR_MITE_UNSUPPORTED_API');
}
for (const options of [
  { level: 100 },
  { windowBits: NaN },
  { chunkSize: 1048577 },
  { maxOutputLength: 67108865 }
]) {
  refuses(() => zlib.createGzip(options), 'ERR_OUT_OF_RANGE');
}

refuses(() => zlib.createGzip({ windowBits: 8 }), 'ERR_OUT_OF_RANGE');

for (const name of ['createUnzip', 'createBrotliCompress', 'createZstdCompress', 'crc32']) {
  refuses(() => zlib[name](), 'ERR_MITE_UNSUPPORTED_API');
}
const codec = zlib.createDeflateRaw();

refuses(() => codec.params(1, 0, () => {}), 'ERR_MITE_UNSUPPORTED_API');

refuses(() => codec.reset(), 'ERR_MITE_UNSUPPORTED_API');

codec.close();
console.log('PASS explicit zlib limitations and option bounds');
