/** State/validation only: native capture is independently tested by the C++ and public gates. */
import createTracing from '../../core/tracing/index.mjs';
import path from 'node:path';

const error = (code, message) => Object.assign(Error(message), { code });

const unsupported = (message) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', message);
};

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

async function rejects(action, code) {
  let actual;
  try {
    await action();
  } catch (error) {
    actual = error.code;
  }
  check(actual === code, `Expected ${code}, got ${actual}`);
}

let ready = false;
let wire;
let starts = 0;
let finishes = 0;
let fail = false;
let release;
let written;
const tracing = createTracing(
  {
    traceCategories: () => '["firefly.main","firefly.window","firefly.ipc"]',

    traceStart(value) {
      wire = JSON.parse(value);
      ++starts;
    },

    traceFinish() {
      ++finishes;
      return 'ACTUAL_SNAPSHOT';
    },

    traceUsage: () => 0.25,

    traceTemporaryPath: () => '/tmp/private/trace.json'
  },
  () => ready,
  {
    promises: {
      async writeFile(output, data, options) {
        written = { output, data, options };
        if (fail) {
          throw error('EIO', 'injected write failure');
        }
        if (release === null) {
          await new Promise((resolve) => {
            release = resolve;
          });
        }
      }
    }
  },
  path,
  error,
  unsupported
);
await rejects(() => tracing.getCategories(), 'ERR_FIREFLY_NOT_READY');
ready = true;
check((await tracing.getCategories()).includes('firefly.main'), 'categories');
await rejects(
  () => tracing.startRecording({ categoryFilter: 'v8' }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);
await rejects(
  () => tracing.startRecording({ traceOptions: 'record-until-full,enable-sampling' }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);
await rejects(
  () => tracing.startRecording({ memory_dump_config: {} }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);
await rejects(
  () => tracing.startRecording({ included_categories: Array(33).fill('*') }),
  'ERR_INVALID_ARG_VALUE'
);
await tracing.startRecording({
  categoryFilter: '*, -firefly.ipc',
  traceOptions: 'record-continuously'
});
check(wire.continuous && wire.excluded[0] === 'firefly.ipc', 'legacy config');
await tracing.startRecording({});
check(starts === 1, 'duplicate start preserves recording');
const usage = await tracing.getTraceBufferUsage();
check(usage.value === 0.25 && usage.percentage === 25, 'usage');
await rejects(() => tracing.stopRecording(4), 'ERR_INVALID_ARG_TYPE');
check(finishes === 0, 'bad path leaves recording active');
fail = true;
await rejects(() => tracing.stopRecording('/out/trace.json'), 'EIO');
await rejects(() => tracing.startRecording({}), 'ERR_FIREFLY_TRACE_BUSY');
fail = false;
release = null;
const save = tracing.stopRecording('trace.json');
await rejects(() => tracing.stopRecording('other.json'), 'ERR_FIREFLY_TRACE_BUSY');
release();
check(
  (await save) === path.resolve('trace.json') &&
    written.data === 'ACTUAL_SNAPSHOT' &&
    finishes === 1,
  'retry keeps snapshot'
);
await rejects(() => tracing.stopRecording(), 'ERR_FIREFLY_TRACE_NOT_RECORDING');
await tracing.startRecording({
  included_categories: ['firefly.window'],
  recording_mode: 'record-until-full'
});
check(wire.included[0] === 'firefly.window' && !wire.continuous, 'modern config');
check(
  (await tracing.stopRecording()) === '/tmp/private/trace.json' && written.options.flag === 'wx',
  'exclusive private default file'
);
console.log(
  'PASS tracing configuration, promises, state, unsupported sampling and failed-output retry'
);
