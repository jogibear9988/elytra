/** Exercise ESM-owned CommonJS contexts, including encoded URLs and package conditions. */
import Module, { createRequire, builtinModules, isBuiltin } from 'node:module';
import bareModule from 'module';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import data from './data.json' with { type: 'json' };
import choice from '#choice';

function check(condition, label) {
  if (!condition) {
    throw new Error(label);
  }
}

const filename = fileURLToPath(import.meta.url);
const require = createRequire(import.meta.url);
check(Module === bareModule && Module === require('module'), 'shared module identity');
check(createRequire === Module.createRequire, 'named export identity');
check(require('node:fs') === fs && require('path') === path, 'shared builtin identity');
check(require.main === undefined, 'ESM entry has no CommonJS main');
check(require('./data.json') === data, 'shared JSON identity');
console.log('PASS module utilities ESM/CommonJS, builtin and JSON identities');

const shared = require('./shared.cjs');
const imported = await import('./shared.cjs');
check(imported.default === shared && shared.parent === filename, 'shared CJS and synthetic parent');
const cacheKey = require.resolve('./shared.cjs');
check(require.cache[cacheKey].exports === shared, 'same CommonJS cache');
check(!Object.hasOwn(require.cache, filename), 'synthetic parent never cached');
const another = createRequire(new URL(import.meta.url));
check(another !== require && another.cache === require.cache, 'distinct requires, shared cache');
shared.count++;
check(another('./shared.cjs').count === 1, 'common instance across requires');
delete require.cache[cacheKey];
check(another('./shared.cjs') !== shared, 'cache deletion reloads');
check(
  require.resolve('fs') === 'fs' && require.resolve('node:fs') === 'node:fs',
  'builtin resolve'
);
console.log('PASS shared CJS cache/reload and uncached synthetic parent');

for (const base of [filename, import.meta.url, new URL(import.meta.url), pathToFileURL(filename)]) {
  check(createRequire(base)('./choice.cjs') === 'require', 'accepted base');
}
const directory = new URL('./', import.meta.url);
check(createRequire(directory)('./choice.cjs') === 'require', 'directory URL with trailing slash');
check(
  createRequire(fileURLToPath(directory))('./choice.cjs') === 'require',
  'directory native separator'
);
const virtual = createRequire(new URL('./missing-parent.cjs?query#hash', import.meta.url));
check(virtual('./choice.cjs') === 'require', 'nonexistent base file and URL query/fragment');
check(choice === 'import' && virtual('#choice') === 'require', 'require package condition');
check(require('fixture-dependency') === 'require', 'node_modules package require condition');
try {
  virtual('#blocked');
  throw new Error('blocked package import accepted');
} catch (error) {
  check(error.code === 'ERR_PACKAGE_IMPORT_NOT_DEFINED', 'blocked package import error');
}
console.log('PASS path/URL/directory/virtual contexts and require package conditions');

if (process.platform === 'win32') {
  // Exercise Node's rooted-without-drive spelling and native trailing backslash.
  const rooted = filename.slice(2);
  // A root-relative Windows path uses the *current* drive, not the drive of
  // filename. Hosted CI keeps checkout and temp directories on different drives.
  const candidate = path.join(path.dirname(path.resolve(rooted)), 'choice.cjs');
  if (fs.existsSync(candidate)) {
    check(createRequire(rooted)('./choice.cjs') === 'require', 'Windows rooted context');
  } else {
    try {
      createRequire(rooted)('./choice.cjs');
      throw new Error('Rooted context unexpectedly used another drive');
    } catch (error) {
      check(error.code === 'MODULE_NOT_FOUND', 'Windows rooted context current-drive error');
    }
  }
  check(
    createRequire(import.meta.dirname + '\\')('./choice.cjs') === 'require',
    'Windows directory'
  );
}

const retainedResolve = createRequire(import.meta.url).resolve;
const retainedRequire = createRequire(import.meta.url);
const collect =
  typeof collectHostGarbage === 'function'
    ? collectHostGarbage
    : typeof gc === 'function'
      ? gc
      : undefined;
if (collect) {
  collect();
}
check(
  retainedResolve('./choice.cjs') === require.resolve('./choice.cjs'),
  'retained resolve context'
);
check(retainedRequire('./choice.cjs') === 'require', 'retained require context after GC');
try {
  retainedResolve('./not-present');
  throw new Error('missing module accepted');
} catch (error) {
  check(
    error.code === 'MODULE_NOT_FOUND' && error.requireStack[0] === filename,
    'synthetic require stack'
  );
}
console.log('PASS retained require.resolve context and synthetic requireStack');
if (collect) {
  console.log('PASS created require/resolve survive actual GC');
}

for (const name of ['fs', 'module', 'path', 'stream/web', 'node:fs', 'node:module']) {
  check(isBuiltin(name), 'recognized available builtin');
}
for (const name of ['', './fs', 'unknown', 'node:node:fs', null, {}, new String('fs')]) {
  check(!isBuiltin(name), 'non-builtin');
}
check(Object.isFrozen(builtinModules) && builtinModules.includes('module'), 'frozen builtin list');
console.log('PASS builtin classification and immutable registry');
