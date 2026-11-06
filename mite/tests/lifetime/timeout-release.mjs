/** Collecting unused signals cancels their native deadlines, not just their JS state. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const refs = Array.from({ length: 32 }, () => new WeakRef(AbortSignal.timeout(10000)));
const native = typeof embeddedHost === 'object' ? embeddedHost : null;
const before = native?.activeTimerCount();
let turns = 0;

function check() {
  collect();
  if (++turns < 10) {
    setImmediate(check);
    return;
  }
  console.log(
    'unused',
    refs.some((ref) => ref.deref() === undefined)
  );
  if (native && native.activeTimerCount() >= before) {
    throw Error('Unreachable deadlines kept native timer handles');
  }
}

setImmediate(check);
