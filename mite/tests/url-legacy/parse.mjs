/** Genuine legacy-url differential: modern WHATWG URL is deliberately not the oracle. */
import assert from 'node:assert/strict';
import { parse, Url, URL } from 'node:url';

const cases = [
  '',
  'relative/file?q=a+b#part',
  '/absolute/path?a=1&a=2',
  '//host/path',
  ['//host/path', false, true],
  ['//u:p@host/path', false, false],
  'HTTP://User:p%40ss@EXAMPLE.com:80/a/../b?q=two words#hash',
  'https://example.com:443',
  'http://example.com:',
  'http://example.com:00080/x',
  'http://127.1/',
  'http://0x7f.1/',
  'http://0177.0.0.1/',
  'http://０１２７.１/',
  'http://[2001:db8::1]:8080?q=x',
  'http://[:::]/path',
  'http://[bad/path',
  'https://faß.de/Grüße',
  'https://例え.テスト/path',
  'http://☃.net/',
  'http://é-.com/',
  'http://a_b.com/',
  'http://a..b/',
  'http://-a.com/',
  'http://a\u200db.com/',
  'http://xn--a/',
  'http://a'.replace('a', 'a'.repeat(70)) + '.com/',
  'http://user@name:p@host/a',
  'http://a@b?@c',
  'http://user%ZZ@host/',
  'http://example.com:bad/path',
  'http://example.com:65536/',
  'http://a%25b/path',
  'http://example.com/a\\b?x=\\#tail\\',
  '  \tHTTP://EXAMPLE.COM/a\r\n ',
  "http://example.com/a'b<>^`{|}",
  'mailto:user@example.com',
  "javascript:alert('hello')",
  'file:///tmp/a%20b',
  'foo:bar/baz?x=y',
  '?x=1',
  '#hash',
  [
    'https://example.com/?a=1&a=2&__proto__=safe&constructor=own&bad=%ZZ&utf8=%E2%82&space=a+b',
    true
  ],
  ['/path?empty=&=value&&plus=%2B&bom=%EF%BB%BF', true],
  ['no-query', true],
  ['?' + Array.from({ length: 1002 }, (_, index) => `n${index}=v`).join('&'), true],
  ['?' + '&'.repeat(1000) + 'last=value', true],
  'http://\ud800.example/',
  'http://\udfff.example/',
  ['/?\ud800=\udfff', true],
  ['/?key=raw\ud800&raw\udfff=value', true],
  ['/?key=raw\ud800%20tail&raw\udfff%2B=value', true],
  ['/?key=%ED%A0%80&%ED%B0%80=value&bad=%C0%AF&incomplete=%E2%82', true],
  ['/?mixed=\ud800%ZZ\udfff&plain=\ud800+\udfff', true],
  ['/?key=\ud800&\udfff=value', false],
  ['/?key=\ud800%20tail&\udfff%2B=value', false],
  ['/?key=\ud800%FF&low=\udfff%C0%AF', true],
  ['/?key=é%FF&pair=𝄞%FF', true],
  ['/?valid=𝄞&pair=%F0%9D%84%9E', true],
  null,
  42,
  {},
  new URL('https://example.com/modern')
];
const rows = [];
for (const entry of cases) {
  const args = Array.isArray(entry) ? entry : [entry];
  try {
    const value = parse(...args);
    assert.ok(value instanceof Url);
    assert.equal(parse(value), value);
    assert.deepEqual(Object.keys(value), [
      'protocol',
      'slashes',
      'auth',
      'host',
      'port',
      'hostname',
      'hash',
      'search',
      'query',
      'pathname',
      'path',
      'href'
    ]);
    rows.push({
      value: { ...value },
      formatted: value.format(),
      queryNullPrototype:
        value.query !== null && typeof value.query === 'object'
          ? Object.getPrototypeOf(value.query) === null
          : null
    });
  } catch (error) {
    if (error.code === 'ERR_ASSERTION') {
      throw error;
    }
    rows.push({ error: error.name, code: error.code ?? null, input: error.input ?? null });
  }
}
const changed = new Url();
changed.protocol = 'http';
changed.hostname = '::1';
changed.port = '8080';
changed.pathname = '/a?b#c';
changed.auth = 'user:p@ss';
changed.query = Object.assign(Object.create(null), { a: ['!', 'two words'], z: true });
rows.push({ mutatedFormat: changed.format() });
console.log(JSON.stringify(rows));
