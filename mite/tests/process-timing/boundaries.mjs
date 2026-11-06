/** Deterministic owned-policy edges supplement actual native clock/CPU acceptance. */
import install from '../../modules/core/process-timing.mjs';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

const failed = new Error('native measurement fixture failed');
failed.code = 'EIO';
failed.errno = -5;
failed.syscall = 'uv_getrusage';
let clock = [18446744073, 709551615];
let fault = false;
const host = {
  monotonicTime() {
    return [...clock];
  },

  processUptime() {
    return 1.25;
  },

  cpuTime() {
    if (fault) {
      throw failed;
    }
    return [100, 200];
  }
};
const p = {};

install(host, p, (code, message, Type = TypeError) => Object.assign(new Type(message), { code }));

check(p.hrtime.bigint() === 18446744073709551615n, 'full unsigned 64-bit precision');
clock = [12, 5];
check(JSON.stringify(p.hrtime([10, 20])) === '[1,999999985]', 'second-boundary nanosecond borrow');
check(p.uptime() === 1.25, 'fractional native uptime preserved');
check(
  JSON.stringify(p.cpuUsage({ user: 150, system: 300 })) === '{"user":-50,"system":-100}',
  'unclamped CPU differences'
);
fault = true;
let caught;
try {
  p.cpuUsage();
} catch (error) {
  caught = error;
}
check(
  caught === failed && caught.code === 'EIO' && caught.syscall === 'uv_getrusage',
  'native failure preserved'
);
console.log(
  'PASS owned timing u64 limit second borrow native fractions deltas and failure identity'
);
console.log('DONE process timing boundaries');
