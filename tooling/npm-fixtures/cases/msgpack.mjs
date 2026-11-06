let encodedCalls = 0;
let decodedCalls = 0;
const encodeInto = TextEncoder.prototype.encodeInto;
const decodeText = TextDecoder.prototype.decode;

TextEncoder.prototype.encodeInto = function (...args) {
  ++encodedCalls;
  return encodeInto.apply(this, args);
};

TextDecoder.prototype.decode = function (...args) {
  ++decodedCalls;
  return decodeText.apply(this, args);
};
// Load the actual published ESM files; the package's bare entry is CommonJS.
const msg = await import('@msgpack/msgpack/dist.esm/index.mjs');
const text = 'stream 文😀 '.repeat(80);

async function* chunks(bytes) {
  for (let i = 0; i < bytes.length; i += 17) {
    await Promise.resolve();
    yield bytes.subarray(i, i + 17);
  }
}

const value = { text, list: [1, true, null], bytes: new Uint8Array([1, 2, 255]) };
const result = await msg.decodeAsync(chunks(msg.encode(value)));
console.log(result.text === text, JSON.stringify(result.list), Array.from(result.bytes).join(','));
const multi = [];
for await (const item of msg.decodeMultiStream(
  chunks(Buffer.concat([msg.encode(text), msg.encode(2), msg.encode([3])]))
)) {
  multi.push(item);
}
console.log(multi[0] === text, multi[1], JSON.stringify(multi[2]));
const array = [];
for await (const item of msg.decodeArrayStream(chunks(msg.encode([text, 1, true])))) {
  array.push(item);
}
console.log(array[0] === text, array[1], array[2]);
console.log(encodedCalls > 0, decodedCalls > 0);
try {
  msg.decode(msg.encode(text), { maxStrLength: 2 });
} catch (error) {
  console.log(error.name);
}
