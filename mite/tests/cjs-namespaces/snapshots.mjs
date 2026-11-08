import value, { count, change } from './mutable.cjs';
import * as namespace from './mutable.cjs';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
change();
if (
  count !== 1 ||
  namespace.count !== 1 ||
  value.count !== 2 ||
  require('./mutable.cjs') !== value ||
  'addedLater' in namespace
) {
  throw Error('Named snapshot versus live CommonJS object');
}
let assignment;
try {
  namespace.count = 9;
} catch (error) {
  assignment = error.name;
}
console.log(
  JSON.stringify({
    named: count,
    defaultCount: value.count,
    added: value.addedLater,
    namespaceAdded: 'addedLater' in namespace,
    assignment
  })
);
