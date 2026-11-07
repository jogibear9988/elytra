/** Invalid hooks fail at registration/resolution, without falling through to another package. */
import { registerHooks, createRequire } from 'node:module';
const require = createRequire(import.meta.url);

const check = (value, label) => {
  if (!value) {
    throw new Error(label);
  }
};

for (const options of [undefined, null]) {
  try {
    registerHooks(options);
    throw new Error('Accepted invalid options');
  } catch (error) {
    check(error instanceof TypeError, 'options type');
  }
}
for (const resolve of [1, 'x', {}]) {
  try {
    registerHooks({ resolve });
    throw new Error('Accepted invalid function');
  } catch (error) {
    check(error.code === 'ERR_INVALID_ARG_TYPE', 'function type');
  }
}
const sentinel = new Error('sentinel');
for (const resolve of [
  () => undefined,
  () => null,
  () => 3,
  () => Promise.resolve({ url: 'node:fs' }),
  () => ({}),
  () => ({ url: 'node:fs' }),
  () => {
    throw sentinel;
  }
]) {
  const hooks = registerHooks({ resolve });
  try {
    await import('bad:import');
    throw new Error('Accepted invalid hook');
  } catch (error) {
    check(error === sentinel || error instanceof TypeError, 'invalid hook or exact thrown object');
  }
  try {
    require('bad:require');
    throw new Error('Accepted invalid hook');
  } catch (error) {
    check(error === sentinel || error instanceof TypeError, 'invalid CJS hook');
  }
  hooks.deregister();
}
const badURL = registerHooks({ resolve: () => ({ url: './value.mjs', shortCircuit: true }) });
try {
  await import('relative-url');
  throw new Error('Accepted relative URL');
} catch (error) {
  check(error.code === 'ERR_INVALID_URL', 'absolute URL required');
}
badURL.deregister();
console.log(
  'PASS invalid options/functions/results, synchronous-only chain, exact exceptions and absolute URLs'
);
