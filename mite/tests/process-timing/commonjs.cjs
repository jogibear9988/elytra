/** CommonJS coverage stays explicit while application examples remain ESM. */
const p = require('node:process');
if (
  p !== process ||
  p !== require('process') ||
  typeof p.hrtime.bigint() !== 'bigint' ||
  !(p.uptime() > 0) ||
  !(p.cpuUsage().user >= 0)
) {
  throw new Error('CommonJS process timing identity');
}
console.log('PASS CommonJS process timing shared identities');
