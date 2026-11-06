import value, { count, increment } from './dep.mjs';
import * as namespace from './dep.mjs';
console.log(value.label, count);
increment();
console.log(count, namespace.count);
console.log(namespace === (await import('./dep.mjs')));
console.log(typeof require, typeof module, typeof __filename);
