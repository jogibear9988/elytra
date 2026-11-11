/** Real public querystring operations; malformed bytes and custom codecs stay observable. */
import assert from 'node:assert/strict';
import query, { parse, decode, stringify, encode, unescapeBuffer } from 'node:querystring';
import { Buffer } from 'node:buffer';
assert.equal(parse, decode);
assert.equal(stringify, encode);
assert.equal(query.parse, parse);
const values = parse('a=1&a=2&__proto__=safe&space=+&broken=%FF');
assert.equal(Object.getPrototypeOf(values), null);
assert.deepEqual(values.a, ['1', '2']);
assert.equal(values.__proto__, 'safe');
assert.equal(values.space, ' ');
assert.equal(values.broken, '\ufffd');
assert.deepEqual({ ...parse('a::1;;a::2', ';;', '::') }, { a: ['1', '2'] });
assert.deepEqual({ ...parse('a=1&&b=2', '&', '=', { maxKeys: 2 }) }, { a: '1' });
assert.equal(
  stringify({ x: ['a b', true, 3n], nil: null, infinity: Infinity }),
  'x=a%20b&x=true&x=3&nil=&infinity='
);
assert.equal(
  stringify({ x: 'hello' }, ';', ':', { encodeURIComponent: (value) => `[${value}]` }),
  '[x]:[hello]'
);
assert.deepEqual(
  { ...parse('key=value', '&', '=', { decodeURIComponent: (value) => value.toUpperCase() }) },
  { KEY: 'VALUE' }
);
assert.equal(query.escape('é +'), '%C3%A9%20%2B');
assert.equal(query.unescape('%FF'), '\ufffd');
const bytes = unescapeBuffer('%FF+%41', true);
assert.equal(Buffer.isBuffer(bytes), true);
assert.deepEqual([...bytes], [255, 32, 65]);

assert.throws(() => query.escape('\ud800'), { name: 'URIError', code: 'ERR_INVALID_URI' });

console.log('PASS public querystring parsing, encoding, codecs and malformed bytes');
