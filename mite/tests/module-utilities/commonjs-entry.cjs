// A legacy entry retains its real main module when creating an additional require.
const { createRequire } = require('node:module');
const contextual = createRequire(__filename);
if (contextual.main !== module || contextual.cache !== require.cache) {
  throw new Error('CommonJS main/cache identity changed');
}
if (contextual('./shared.cjs').parent !== __filename) {
  throw new Error('Synthetic parent filename changed');
}
console.log('PASS CommonJS entry main/cache identity with created require');
