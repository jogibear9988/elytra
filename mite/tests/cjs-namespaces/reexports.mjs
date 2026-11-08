/** Relative source reexports are recursive and cycle-safe; builtin bridges stay default-only. */
import value, { left, right } from './chain.cjs';
import * as first from './cycle-a.cjs';
import * as second from './cycle-b.cjs';
import * as builtin from './builtin.cjs';
import path from 'node:path';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
if (left !== 10 || right !== 20 || require('./leaf.cjs') !== value) {
  throw Error('Relative reexport chain');
}
if (
  first.a !== 'A' ||
  first.b !== undefined ||
  second.a !== 'A' ||
  second.b !== undefined ||
  first.default !== second.default
) {
  throw Error('Cycle export-name analysis and execution identity');
}
if (builtin.default !== path || builtin['module.exports'] !== path || 'join' in builtin) {
  throw Error('Builtin reexport keeps actual default-only namespace');
}
console.log(
  JSON.stringify({
    values: [left, right],
    cycleKeys: [Object.keys(first), Object.keys(second)],
    cycleValue: [first.a, first.b ?? null],
    builtinKeys: Object.keys(builtin)
  })
);
