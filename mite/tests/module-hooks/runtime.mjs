/** Real module execution across resolve chains, caches, attributes and reentrant resolution. */
import { registerHooks, createRequire } from 'node:module';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
const require = createRequire(import.meta.url);
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;

const check = (value, label) => {
  if (!value) {
    throw new Error(label);
  }
};

const logs = [];
const first = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('test:')) {
      logs.push('first');
      check(context.conditions.includes('node'), 'native condition');
      check(context.parentURL === import.meta.url, 'parent URL');
      check(
        context.importAttributes === undefined
          ? context.conditions.includes('require')
          : context.conditions.includes('import'),
        'loader context'
      );
      const target = new URL(
        specifier.endsWith('cjs') ? './value.cjs' : './value.mjs',
        import.meta.url
      );
      return nextResolve(
        context.importAttributes === undefined ? fileURLToPath(target) : target.href,
        context
      );
    }
    return nextResolve(specifier, context);
  }
});
const second = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('test:')) {
      logs.push('second');
    }
    return nextResolve(specifier);
  }
});
const imported = await import('test:esm');
check(imported.default === 'esm' && logs.join() === 'second,first', 'ESM LIFO chain');
logs.length = 0;
check(require('test:cjs').value === 'cjs' && logs.join() === 'second,first', 'CJS LIFO chain');
check(
  require.resolve('test:cjs') === fileURLToPath(new URL('./value.cjs', import.meta.url)),
  'require.resolve mapping'
);
check(
  require.resolve('fs') === 'fs' && require.resolve('node:fs') === 'node:fs',
  'builtin resolve identity'
);
second.deregister();
second.deregister();
logs.length = 0;
check(
  (await import('test:again')).default === 'esm' && logs.join() === 'first',
  'idempotent deregistration'
);
check(imported === (await import('test:cache')), 'canonical ESM cache');
check(require('test:cjs') === require('./value.cjs'), 'canonical CommonJS cache');
first.deregister();
console.log('PASS ESM/CommonJS resolve chains, context, deregistration and real cache identities');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'reentrant') {
      check(require('builtin') === fs, 'nested CommonJS resolver');
      return nextResolve('./value.mjs', context);
    }
    if (specifier === 'graph-child') {
      return nextResolve('./value.mjs', context);
    }
    if (specifier === 'json-next') {
      return nextResolve('./data.json', { ...context, importAttributes: { type: 'json' } });
    }
    if (specifier === 'short') {
      return {
        url: new URL('./value.mjs?short', import.meta.url).href,
        format: 'module',
        shortCircuit: true
      };
    }
    if (specifier === 'json') {
      return {
        url: new URL('./data.json', import.meta.url).href,
        importAttributes: { type: 'json' },
        shortCircuit: true
      };
    }
    if (specifier === 'condition') {
      return nextResolve('conditional', {
        ...context,
        conditions: [...context.conditions, 'custom']
      });
    }
    if (specifier === 'parent') {
      return nextResolve('./relative.mjs', {
        ...context,
        parentURL: new URL('./nested/main.mjs', import.meta.url).href
      });
    }
    if (specifier === 'builtin') {
      return { url: 'node:fs', shortCircuit: true };
    }
    return nextResolve(specifier, context);
  }
});
if (typeof collect === 'function') {
  collect();
}
check((await import('reentrant')).default === 'esm', 'nested native resolution');
check((await import('./graph.mjs')).default === 'esm', 'static graph resolution');
try {
  await import('json-next');
  throw new Error('nextResolve context attributes became return attributes');
} catch (error) {
  check(error.code === 'ERR_IMPORT_ATTRIBUTE_MISSING', 'attributes must be returned explicitly');
}
check((await import('short')).default === 'esm', 'short circuit and format');
check((await import('json')).default.ok === true, 'effective JSON attributes');
check((await import('condition')).default === 'custom', 'ESM custom export condition');
check(require('condition') === 'custom-cjs', 'CJS custom export condition');
check((await import('parent')).default === 'nested', 'changed parent URL');
check((await import('builtin')).default === fs && require('builtin') === fs, 'builtin mapping');
hook.deregister();
console.log(
  'PASS short circuit, JSON attributes, added export conditions, changed parent and builtin mapping'
);

// A saved continuation remains callable after its original native resolver has returned.
let saved;
const retained = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'retain') {
      saved = nextResolve;
      return nextResolve('./value.mjs', context);
    }
    return nextResolve(specifier, context);
  }
});
await import('retain');
retained.deregister();
if (typeof collect === 'function') {
  collect();
}
check(
  saved('./value.mjs').url === new URL('./value.mjs', import.meta.url).href,
  'retained continuation'
);
console.log('PASS retained continuation remains valid after deregistration and available GC');

if (typeof collect === 'function') {
  console.log('PASS registered hooks and retained continuation survive actual GC');
}
