/** Match Node's argument error classification before filesystem access. */
import { createRequire } from 'node:module';

const lookalike = {
  get href() {
    return import.meta.url;
  }
};
const invalid = [
  undefined,
  null,
  17,
  false,
  Symbol('x'),
  {},
  lookalike,
  new String(import.meta.url),
  '',
  './relative.cjs',
  'https://example.test/main.cjs',
  new URL('https://example.test/main.cjs'),
  'file:///tmp/a%2fb'
];
for (const value of invalid) {
  let denied = false;
  try {
    createRequire(value);
  } catch (error) {
    denied = error instanceof TypeError && error.code === 'ERR_INVALID_ARG_VALUE';
  }
  if (!denied) {
    throw new Error('Invalid createRequire argument accepted');
  }
}
console.log('PASS createRequire argument and URL identity validation');

const require = createRequire(import.meta.url);
for (const method of [require, require.resolve]) {
  for (const value of [undefined, 1, {}, Symbol('x')]) {
    let denied = false;
    try {
      method(value);
    } catch (error) {
      denied = error instanceof TypeError && error.code === 'ERR_INVALID_ARG_TYPE';
    }
    if (!denied) {
      throw new Error('Invalid module ID accepted');
    }
  }
}
console.log('PASS generated require and resolve argument errors');
