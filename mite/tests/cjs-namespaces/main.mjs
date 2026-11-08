/** Namespace names come from source lexing, while default remains the real CommonJS export object. */
import value, { dot, bracket, direct, escaped, defined } from './lexical.cjs';
import * as namespace from './lexical.cjs';
import callable, { answer } from './function.cjs';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

check(
  value === require('./lexical.cjs') &&
    namespace.default === value &&
    namespace['module.exports'] === value,
  'Default/module.exports/require object identity'
);
check(
  value.default === 'inner-default' && value['module.exports'] === 'inner-marker',
  'CommonJS own properties cannot shadow the namespace default or module.exports marker'
);
check(
  dot === 1 &&
    direct === 2 &&
    bracket === 3 &&
    escaped === 5 &&
    defined === 6 &&
    namespace['hyphen-name'] === 4,
  'Lexical named exports'
);
check(
  value.computed === 7 &&
    value.dynamicName === 8 &&
    !('computed' in namespace) &&
    !('dynamicName' in namespace),
  'Computed properties stay on the default object only'
);
for (const name of ['comment', 'blockComment', 'string', 'template', 'regex', 'getterOnly']) {
  check(!(name in namespace), 'Do not infer exports from non-code text or unrecognized getters');
}
check(globalThis.getterOnlyReads === undefined, 'Unrecognized getter is never evaluated');
check(
  globalThis.throwingReads === 1 && namespace.throwing === undefined,
  'Detected throwing getter is attempted once and its exception is swallowed'
);
check(namespace === (await import('./lexical.cjs')), 'ESM namespace cache identity');
check(
  callable() === 'callable' && answer === 42 && require('./function.cjs') === callable,
  'Function-valued CommonJS export identity'
);
console.log(
  JSON.stringify({
    keys: Object.keys(namespace),
    descriptor: Object.getOwnPropertyDescriptor(namespace, 'dot'),
    values: [dot, direct, bracket, escaped, defined, namespace['hyphen-name']],
    getters: [globalThis.getterOnlyReads ?? 0, globalThis.throwingReads],
    callable: callable(),
    answer
  })
);
