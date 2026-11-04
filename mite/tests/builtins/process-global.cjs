const original = process;
globalThis.process = { fake: true };
const builtin = require('node:process');
console.log(builtin === original, builtin.fake === undefined);
globalThis.process = original;
