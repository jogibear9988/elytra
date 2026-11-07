/** Explicit implementation boundaries must fail before installing a partial hook. */
import { registerHooks } from 'node:module';

function check(value, label) {
  if (!value) {
    throw new Error(label);
  }
}

let called = false;
try {
  registerHooks({
    resolve() {
      called = true;
    },

    load() {}
  });
  throw new Error('load accepted');
} catch (error) {
  check(error.code === 'ERR_MITE_UNSUPPORTED_API', 'load refusal');
}
await import('./value.mjs');
check(!called, 'atomic registration refusal');
for (const url of [
  'mite:internal/cjs',
  'mite:internal/builtin',
  'mite:internal/host',
  'mite-host:missing',
  'data:text/javascript,export default 1',
  'https://example.invalid/x'
]) {
  const hooks = registerHooks({ resolve: () => ({ url, shortCircuit: true }) });
  try {
    await import('private-url');
    throw new Error('unsupported URL accepted');
  } catch (error) {
    check(error.code === 'ERR_UNSUPPORTED_ESM_URL_SCHEME', 'scheme refusal');
  }
  hooks.deregister();
}
const override = registerHooks({
  resolve: () => ({
    url: new URL('./value.mjs', import.meta.url).href,
    format: 'commonjs',
    shortCircuit: true
  })
});
try {
  await import('override');
  throw new Error('format override accepted');
} catch (error) {
  check(error.code === 'ERR_MITE_UNSUPPORTED_API', 'format refusal');
}
override.deregister();
const hooks = Array.from({ length: 64 }, () =>
  registerHooks({
    resolve(s, c, n) {
      return n(s, c);
    }
  })
);
try {
  registerHooks({ resolve() {} });
  throw new Error('limit accepted');
} catch (error) {
  check(error.code === 'ERR_MITE_RESOLVE_HOOK_LIMIT', 'bounded registration');
}

hooks.forEach((h) => h.deregister());

console.log(
  'PASS unsupported load/format/scheme denial, private bridge protection and bounded atomic registration'
);
