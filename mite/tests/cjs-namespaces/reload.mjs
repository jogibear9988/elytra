import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const filename = require.resolve('./reload.cjs');
const first = require(filename);
delete require.cache[filename];
const second = require(filename);
const namespace = await import('./reload.cjs');
if (
  first === second ||
  first.instance !== 1 ||
  second.instance !== 2 ||
  namespace.default !== second ||
  namespace.instance !== 2
) {
  throw Error('Successful CommonJS reload refreshes the original-export namespace bridge');
}
console.log(
  JSON.stringify({
    first: first.instance,
    second: second.instance,
    named: namespace.instance,
    defaultSecond: namespace.default === second
  })
);
