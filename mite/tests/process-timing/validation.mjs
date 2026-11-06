/** Stable error/type/coercion observations compared with actual pinned Node. */
import { hrtime, cpuUsage } from 'node:process';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function fails(fn, name, code) {
  let failure;
  try {
    fn();
  } catch (error) {
    failure = error;
  }
  check(failure?.name === name && failure.code === code, 'error contract ' + name + ':' + code);
}

for (const value of [null, false, 1, 'x', {}, new Uint32Array(2)]) {
  fails(() => hrtime(value), 'TypeError', 'ERR_INVALID_ARG_TYPE');
}
for (const value of [[], [0], [0, 0, 0]]) {
  fails(() => hrtime(value), 'RangeError', 'ERR_OUT_OF_RANGE');
}
for (const value of [undefined, null, false, 0, NaN, '', 0n]) {
  const used = cpuUsage(value);
  check(used.user >= 0 && used.system >= 0, 'falsy CPU snapshot ignored');
}
for (const value of [
  {},
  [],
  true,
  1,
  'x',
  { user: 0 },
  { user: '0', system: 0 },
  { user: 0, system: 1n }
]) {
  fails(() => cpuUsage(value), 'TypeError', 'ERR_INVALID_ARG_TYPE');
}
for (const field of ['user', 'system']) {
  for (const value of [-1, NaN, Infinity, -Infinity, Number.MAX_SAFE_INTEGER + 1]) {
    fails(
      () => cpuUsage({ user: 0, system: 0, [field]: value }),
      'RangeError',
      'ERR_INVALID_ARG_VALUE'
    );
  }
}
const fractional = cpuUsage({ user: 0.5, system: 0.5 });
check(
  Math.abs(fractional.user % 1) === 0.5 && Math.abs(fractional.system % 1) === 0.5,
  'fractional CPU subtraction'
);
console.log('PASS process timing tuple validation and CPU type range falsy fractional contracts');

let trace = '';
cpuUsage({
  get user() {
    trace += 'u';
    return 0;
  },

  get system() {
    trace += 's';
    return 0;
  }
});
check(trace === 'usus', 'valid CPU getter access order');
const reason = new Error('measurement getter refused');
for (const fn of [
  () =>
    cpuUsage({
      get user() {
        throw reason;
      }
    }),
  () =>
    hrtime(
      new Proxy([0, 0], {
        get() {
          throw reason;
        }
      })
    )
]) {
  let caught;
  try {
    fn();
  } catch (error) {
    caught = error;
  }
  check(caught === reason, 'getter rejection identity');
}
const coerced = hrtime(['0', '0']);
check(Number.isFinite(coerced[0]) && Number.isFinite(coerced[1]), 'legacy arithmetic coercion');
check(typeof hrtime.bigint('ignored') === 'bigint', 'bigint ignores legacy difference input');
console.log('PASS process timing valid getters exception identity and legacy tuple coercion');
console.log('DONE process timing validation');
