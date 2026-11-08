/** Direct native boundary checks; public singleton/selected engine semantics have another gate. */
import { appLocaleRead as read, markReady } from 'locale-test';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let actual;
  try {
    action();
  } catch (error) {
    actual = error.code;
  }
  check(actual === code, `Expected ${code}, got ${actual}`);
}

for (const value of [undefined, null, '1', {}, 1.5, NaN, Infinity]) {
  fails(() => read(value), 'ERR_INVALID_ARG_TYPE');
}
for (const value of [-1, 4, 100]) {
  fails(() => read(value), 'ERR_INVALID_ARG_VALUE');
}

fails(() => read(1), 'ERR_FIREFLY_NOT_READY');

fails(() => read(2), 'ERR_FIREFLY_NOT_READY');

const languages = read(0);
check(
  Array.isArray(languages) && languages.every((x) => typeof x === 'string'),
  'native languages'
);
languages.push('private-test-sentinel');
check(!read(0).includes('private-test-sentinel'), 'array alias');
check(typeof read(3) === 'string', 'early country');
markReady();
check(typeof read(1) === 'string' && read(1).length > 0, 'regional locale');
check(read(2) === 'sr-Latn-RS', 'native selected application locale');
console.log(
  'PASS native locale selectors/readiness/fresh arrays and independent selected language'
);
