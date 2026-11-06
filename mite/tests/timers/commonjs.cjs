/** Deliberate CJS module/global identity and eager-capture compatibility fixture. */
const original = globalThis.setImmediate;
globalThis.setImmediate = null;
const timers = require('node:timers');
console.log(timers === require('timers'), timers.setImmediate === original);

timers.setImmediate(() => console.log('commonjs immediate'));
