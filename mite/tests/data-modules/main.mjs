import assert from 'node:assert/strict';
import { Buffer } from 'node:buffer';
import { registerHooks } from 'node:module';

const data = (source) => 'data:text/javascript,' + encodeURIComponent(source);

const load = (source) => import(data(source));

const requireGC = process.argv.includes('--require-gc');
if (requireGC) {
  assert.equal(typeof globalThis.gc, 'function');
}

const body = 'export default "Grüße 🌍";';
for (const url of [
  data(body),
  'data:text/javascript;base64,' + Buffer.from(body).toString('base64'),
  'data:text/javascript;base64,' + Buffer.from(body).toString('base64').replace(/=+$/, ''),
  'data:text/javascript; charset=utf-8,' + encodeURIComponent(body),
  'data:text/javascript;base64,' +
    encodeURIComponent(' \n' + Buffer.from(body).toString('base64') + '\t')
]) {
  assert.equal((await import(url)).default, 'Grüße 🌍');
}
console.log('PASS data module percent/base64/MIME decoding');

const identity = data('export const object = {}; export const url = import.meta.url;');
const first = await import(identity + '#first');
assert.equal(await import(identity + '#first'), first);
const second = await import(identity + '#second');
assert.notEqual(first.object, second.object);
assert.equal(first.url, identity + '#first');
assert.equal(second.url, identity + '#second');
const queried = data('export default import.meta.url; //') + '?literal-query';
assert.equal((await import(queried)).default, queried);
assert.equal(
  (await import('data:text/javascript,export%20default%20%22body?query%22')).default,
  'body?query'
);
const meta = await load(
  'export default {url:import.meta.url,main:import.meta.main,filename:import.meta.filename,dirname:import.meta.dirname};'
);
assert.equal(meta.default.main, false);
assert.equal(meta.default.filename, undefined);
assert.equal(meta.default.dirname, undefined);
assert.ok(meta.default.url.startsWith('data:text/javascript,'));
console.log('PASS data module identity and real import metadata');

const json = 'data:application/json,' + encodeURIComponent('{"owned":[1,"two"]}');
assert.deepEqual((await import(json, { with: { type: 'json' } })).default, { owned: [1, 'two'] });
assert.deepEqual(
  (
    await import('data:application/json,%EF%BB%BF%7B%22bom%22%3Atrue%7D', {
      with: { type: 'json' }
    })
  ).default,
  { bom: true }
);
await assert.rejects(import(json));
await assert.rejects(import(data('export default 1'), { with: { type: 'json' } }));
assert.deepEqual(
  (
    await import('data:application/json;charset=utf-8,%7B%22parameter%22%3Atrue%7D', {
      with: { type: 'json' }
    })
  ).default,
  { parameter: true }
);
for (const mime of ['APPLICATION/JSON', ' application/json', 'application/json ']) {
  await assert.rejects(import('data:' + mime + ',{}', { with: { type: 'json' } }), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT'
  });
}
console.log('PASS data JSON import attributes');

const file = new URL('./absolute.mjs', import.meta.url).href;
const imports = await load(
  `import * as bare from 'fs'; import * as explicit from 'node:fs'; import {value} from ${JSON.stringify(file)}; export default [bare===explicit,value];`
);
assert.deepEqual(imports.default, [true, 'absolute-file']);
await assert.rejects(load('import "./absolute.mjs";'));
await assert.rejects(load('import "owned-nonexistent-data-module-package";'));
console.log('PASS data module imports preserve parent resolution');

let parentSeen = false;
let attrsSeen = false;
const replacement = data('export default "hooked";');
const hooks = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'owned-data-hook') {
      assert.equal(context.parentURL, import.meta.url);
      parentSeen = true;
      return { url: replacement, format: 'module', shortCircuit: true };
    }
    if (specifier === 'owned-json-hook') {
      assert.equal(context.importAttributes.type, 'json');
      attrsSeen = true;
      return { url: json, format: 'json', shortCircuit: true };
    }
    return nextResolve(specifier, context);
  }
});
try {
  assert.equal((await import('owned-data-hook')).default, 'hooked');
  assert.deepEqual((await import('owned-json-hook', { with: { type: 'json' } })).default, {
    owned: [1, 'two']
  });
  assert.equal(parentSeen, true);
  assert.equal(attrsSeen, true);
} finally {
  hooks.deregister();
}
console.log('PASS resolve hooks route real data modules and attributes');

// Same mechanism as the original CodeOSS API resolver: generated ESM obtains
// actual exports from an extension-owned global registry, with a file parent.
const key = '__ownedDataModuleRegistry';
const api = {
  marker: {},
  called: 0,

  command() {
    this.called++;
    return this.marker;
  }
};
globalThis[key] = new Map([['owned-extension', api]]);
const registryHook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier !== 'owned-api') {
      return nextResolve(specifier, context);
    }
    assert.equal(context.parentURL, import.meta.url);
    const source = `const api=globalThis[${JSON.stringify(key)}].get('owned-extension'); export default api; export const marker=api.marker;`;
    return {
      url: 'data:text/javascript;base64,' + Buffer.from(source).toString('base64'),
      shortCircuit: true
    };
  }
});
try {
  if (requireGC) {
    globalThis.gc();
  }
  const module = await import('owned-api');
  assert.equal(module.default, api);
  assert.equal(module.marker, api.marker);
  assert.equal(module.default.command(), api.marker);
  assert.equal(api.called, 1);
} finally {
  registryHook.deregister();
  delete globalThis[key];
}
console.log('PASS generated API registry exports preserve real identity');

for (const url of [
  'data:text/plain,export default 1',
  'data:text/javascript;base64,%%%%',
  'data:text/javascript'
]) {
  await assert.rejects(import(url));
}
console.log('PASS malformed data modules reject');

if (requireGC) {
  const catalog = (await import(json, { with: { type: 'json' } })).default;
  globalThis.gc();
  assert.equal((await import(json, { with: { type: 'json' } })).default, catalog);
  assert.equal((await import(identity + '#first')).object, first.object);
  assert.deepEqual(catalog, { owned: [1, 'two'] });
  console.log('PASS data module and JSON caches survive actual native GC');
}
