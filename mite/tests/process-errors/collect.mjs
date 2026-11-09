/** CLI behavior and real collector acceptance are explicitly separate harness roles. */
import assert from 'node:assert/strict';

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
if (typeof collect !== 'function') {
  assert.ok(process.versions.mite, 'Run the Node reference with --expose-gc');
  assert.equal(
    process.env.MITE_PROCESS_ERRORS_GC,
    'cli-without-gc',
    'Collector required outside explicit CLI behavior mode'
  );
}
export default typeof collect === 'function' ? collect : null;
