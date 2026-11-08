/** Failed reexport metadata analysis must not cache a provisional, incomplete export list. */
import fs from 'node:fs';

const root = new URL('./analysis-retry/', import.meta.url);
const child = new URL('./analysis-retry/child/', import.meta.url);
fs.mkdirSync(child, { recursive: true });
fs.writeFileSync(new URL('entry.cjs', root), "module.exports = require('./child/value.js');\n");
fs.writeFileSync(new URL('value.js', child), 'exports.named = 7;\n');
// The runner reuses this source directory in separate Node and Mite processes.
// Always restore the failing initial state; never inherit the previous run's fix.
fs.writeFileSync(new URL('package.json', child), '{invalid');
let failure;
try {
  await import('./analysis-retry/entry.cjs');
} catch (error) {
  failure = error.code;
}
if (failure !== 'ERR_INVALID_PACKAGE_CONFIG') {
  throw Error('First source analysis must fail on the actual invalid package config');
}
fs.writeFileSync(new URL('package.json', child), '{"type":"commonjs"}');
const namespace = await import('./analysis-retry/entry.cjs?retry');
if (namespace.named !== 7 || namespace.default.named !== 7) {
  throw Error('Failed analysis must not leave provisional names in the native cache');
}
console.log(JSON.stringify({ failure, keys: Object.keys(namespace), named: namespace.named }));
