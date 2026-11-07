/** Public ESM option/snapshot policy under Node and Mite, separate from real native crash gates. */
import createReporter from '../../core/crash/index.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  try {
    action();
  } catch (value) {
    check(value.code === code, String(value));
    return;
  }
  throw Error(`Expected ${code}`);
}

let calls = 0;
let saved;
const native = {
  crashStart(wire) {
    ++calls;
    saved = JSON.parse(wire);
  },

  crashParameters() {
    return JSON.stringify(saved.extra);
  }
};
const reporter = createReporter(native, error, unsupported);

fails(() => reporter.getParameters(), 'ERR_FIREFLY_NOT_READY');

fails(() => reporter.start(undefined), 'ERR_INVALID_ARG_TYPE');

fails(() => reporter.start({}), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => reporter.start({ uploadToServer: true }), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(
  () => reporter.start({ uploadToServer: false, ignoreSystemCrashHandler: true }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

fails(() => reporter.start({ uploadToServer: false, compress: 1 }), 'ERR_INVALID_ARG_TYPE');

fails(() => reporter.start({ uploadToServer: false, productName: null }), 'ERR_INVALID_ARG_TYPE');

fails(() => reporter.start({ uploadToServer: false, extra: { bad: 1 } }), 'ERR_INVALID_ARG_TYPE');

fails(
  () => reporter.start({ uploadToServer: false, extra: { 'bad\nkey': 'x' } }),
  'ERR_INVALID_ARG_VALUE'
);

fails(
  () => reporter.start({ uploadToServer: false, globalExtra: { x: 'a'.repeat(2049) } }),
  'ERR_OUT_OF_RANGE'
);

fails(
  () => reporter.start({ uploadToServer: false, unknown: true }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

check(calls === 0, 'invalid input reached native start');
const options = {
  uploadToServer: false,
  extra: { initial: 'yes' },
  globalExtra: { shared: 'yes' }
};
reporter.start(options);
options.extra.initial = 'mutated';
check(calls === 1 && reporter.getParameters().initial === 'yes', 'snapshot ownership');
const copy = reporter.getParameters();
copy.initial = 'mutated';
check(reporter.getParameters().initial === 'yes', 'returned parameters mutated native state');
check(!reporter.getUploadToServer(), 'upload state');

fails(() => reporter.setUploadToServer(true), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => reporter.setUploadToServer(1), 'ERR_INVALID_ARG_TYPE');

check(reporter.setUploadToServer(false) === undefined, 'disabled upload return');

fails(() => reporter.start(options), 'ERR_FIREFLY_CRASH_REPORTER_STARTED');

let attempts = 0;
const retry = createReporter(
  {
    crashStart() {
      if (++attempts === 1) {
        throw Error('native validation');
      }
    }
  },
  error,
  unsupported
);
try {
  retry.start({ uploadToServer: false });
} catch {}
retry.start({ uploadToServer: false });
check(attempts === 2, 'JS failure consumed start before native success');
console.log(
  'PASS local crash reporter option/annotation snapshots, explicit refusals and native failure policy'
);
