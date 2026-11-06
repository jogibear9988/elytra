/** Bare, node-prefixed, default and named module exports share the original functions. */
import timers, { setImmediate as immediate, clearImmediate as clear } from 'node:timers';
import bare from 'timers';
console.log(timers === bare, immediate === setImmediate, clear === clearImmediate);
const original = globalThis.setImmediate;

globalThis.setImmediate = () => {
  throw Error('Replacement must not change the builtin');
};

console.log((await import('node:timers')).setImmediate === original);

timers.setImmediate(() => console.log('module immediate'));
