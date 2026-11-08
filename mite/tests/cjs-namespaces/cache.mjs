/** Node's ESM bridge retains the originally loaded CJS object when require.cache exports is replaced. */
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const filename = require.resolve('./cache.cjs');
const original = require(filename);
const replacement = { named: 'replacement', replacementOnly: 2 };
require.cache[filename].exports = replacement;
const namespace = await import('./cache.cjs');
if (
  namespace.default !== original ||
  namespace.default === require(filename) ||
  namespace.named !== 'initial' ||
  namespace.originalOnly !== 1 ||
  'replacementOnly' in namespace
) {
  throw Error('Cached namespace keeps the original CommonJS export object');
}
original.named = 'after';
if (
  namespace.named !== 'initial' ||
  namespace.default.named !== 'after' ||
  namespace !== (await import('./cache.cjs'))
) {
  throw Error('Cached namespace named snapshot versus original live object');
}
console.log(
  JSON.stringify({
    keys: Object.keys(namespace),
    named: namespace.named,
    defaultNamed: namespace.default.named,
    requiredNamed: require(filename).named
  })
);
