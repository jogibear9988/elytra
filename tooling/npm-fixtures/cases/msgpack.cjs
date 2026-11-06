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

const msg = require('@msgpack/msgpack');
const text = 'Grüße 文😀 '.repeat(100);
const value = {
  text,
  numbers: [-33, 127, 128, 65536, -1099511627776, Math.PI],
  bytes: new Uint8Array([0, 255, 127]),
  date: new Date('2024-01-02T03:04:05.678Z')
};
const result = msg.decode(msg.encode(value, { sortKeys: true }));
console.log(
  result.text === text,
  JSON.stringify(result.numbers) === JSON.stringify(value.numbers),
  Array.from(result.bytes).join(','),
  result.date.toISOString()
);
console.log(encodedCalls > 0, decodedCalls > 0);
console.log(Buffer.from(msg.encode({ a: 1, b: 'x' }, { sortKeys: true })).toString('hex'));
const big = [-9223372036854775808n, 18446744073709551615n];
console.log(
  msg
    .decode(msg.encode(big, { useBigInt64: true }), { useBigInt64: true })
    .map(String)
    .join(',')
);
console.log(
  JSON.stringify([
    ...msg.decodeMulti(Buffer.concat([msg.encode(1), msg.encode('two'), msg.encode([3])]))
  ])
);
for (const data of [
  new Uint8Array([0xc1]),
  new Uint8Array([0xd9, 10, 0x61]),
  msg.encode(JSON.parse('{"__proto__":1}'))
]) {
  try {
    msg.decode(data);
  } catch (error) {
    console.log(error.name, typeof error.message === 'string');
  }
}
console.log(require('@msgpack/msgpack/package.json').version);
