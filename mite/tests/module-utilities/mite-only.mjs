/** Available builtin reporting is narrower than Node's reserved builtin namespace. */
import { createRequire, builtinModules, isBuiltin } from 'node:module';
import { fileURLToPath } from 'node:url';
const require = createRequire(import.meta.url);
if (new Set(builtinModules).size !== builtinModules.length) {
  throw new Error('Duplicate builtin');
}
for (const name of builtinModules) {
  if (!isBuiltin(name) || !isBuiltin(`node:${name}`)) {
    throw new Error('Missing classification');
  }
  if ((await import(`node:${name}`)).default !== require(name)) {
    throw new Error('Builtin unavailable');
  }
}
for (const name of ['v8', 'http2', 'worker_threads']) {
  if (isBuiltin(name) || builtinModules.includes(name)) {
    throw new Error('Unavailable builtin advertised');
  }
  try {
    require(name);
    throw new Error('Unsupported builtin loaded');
  } catch (error) {
    if (error.code !== 'ERR_MITE_UNSUPPORTED_BUILTIN') {
      throw error;
    }
  }
}
try {
  createRequire(fileURLToPath(import.meta.url) + '\0');
  throw new Error('Null path accepted');
} catch (error) {
  if (error.code !== 'ERR_INVALID_ARG_VALUE') {
    throw error;
  }
}
console.log('PASS actual available builtin inventory, reserved-name denial and NUL rejection');
