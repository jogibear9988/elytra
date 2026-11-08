/** JS policy unit with controlled metadata, never OS enumeration or pixel capture acceptance. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const valid = () => ({ types: ['screen', 'window'], thumbnailSize: { width: 0, height: 0 } });

const records = [
  { id: 'screen:7:0', name: 'Controlled display', display_id: '7' },
  { id: 'window:42:0', name: 'Controlled window ä', display_id: '' }
];

async function rejected(action, code) {
  let promise;
  try {
    promise = action();
  } catch {
    throw Error('getSources must reject asynchronously rather than throw');
  }
  check(promise instanceof Promise, 'getSources returns a Promise');
  let failure;
  try {
    await promise;
  } catch (error) {
    failure = error;
  }
  check(failure instanceof Error, 'Explicit policy rejection');
  if (code === null) {
    check(
      failure.code === undefined && failure.message === 'Invalid options',
      'Observed Electron invalid-options error'
    );
  } else if (code) {
    check(failure.code === code, 'Expected ' + code + ', got ' + failure.code);
  }
  return failure;
}

function fixture() {
  const state = { handler: null, starts: [], cancels: [], onStart: null, exited: [] };
  const native = {
    appPath: process.cwd(),

    setHandler(handler) {
      state.handler = handler;
    },

    desktopCaptureStart(id) {
      state.starts.push(id);
      state.onStart?.(id);
    },

    desktopCaptureCancel(id) {
      state.cancels.push(id);
    },

    exit(code) {
      state.exited.push(code);
    }
  };
  const api = createApi(native, EventEmitter, path, urls);
  return {
    api,
    state,

    ready() {
      state.handler(0, 'ready', '');
    },

    result(id, sources = records) {
      state.handler(0, 'desktop-capture-result', JSON.stringify({ id, sources }));
    },

    failure(id, code, message) {
      state.handler(0, 'desktop-capture-result', JSON.stringify({ id, code, message }));
    }
  };
}

const first = fixture();
await rejected(() => first.api.desktopCapturer.getSources(valid()), 'ERR_FIREFLY_NOT_READY');
check(first.state.starts.length === 0, 'Readiness rejection never enters provider');
first.ready();
await first.api.app.whenReady();
for (const options of [
  undefined,
  null,
  42,
  {},
  { types: null },
  { types: 'screen' },
  { ...valid(), types: ['audio'] },
  { ...valid(), types: [1] },
  { ...valid(), types: Array(1) },
  { ...valid(), thumbnailSize: { width: -1, height: 0 } },
  { ...valid(), thumbnailSize: { width: 0.5, height: 0 } },
  { ...valid(), fetchWindowIcons: 'false' }
]) {
  await rejected(() => first.api.desktopCapturer.getSources(options), null);
}
for (const options of [
  { types: ['screen'] },
  { ...valid(), thumbnailSize: { width: 150, height: 150 } },
  { ...valid(), fetchWindowIcons: true },
  { ...valid(), unknownCaptureOption: true }
]) {
  await rejected(
    () => first.api.desktopCapturer.getSources(options),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );
}
check(first.state.starts.length === 0, 'Invalid/unavailable options never enter provider');
console.log('PASS desktop capture readiness and no-provider option policy');

first.state.onStart = (id) => first.result(id);

const options = valid();
const pending = first.api.desktopCapturer.getSources(options);
options.types[0] = 'mutated';
options.thumbnailSize.width = 999;
const sources = await pending;
check(
  sources.length === 2 && sources[0].id === records[0].id && sources[1].name === records[1].name,
  'Canonical controlled metadata'
);
for (const source of sources) {
  const image = source.thumbnail;
  check(
    source.appIcon === null && image.isEmpty(),
    'Metadata-only result has an actual empty image'
  );
  check(JSON.stringify(image.getSize()) === '{"width":0,"height":0}', 'Empty image dimensions');
  for (const bytes of [image.toPNG(), image.toJPEG(80), image.toBitmap(), image.getBitmap()]) {
    check(
      Buffer.isBuffer(bytes) && bytes.length === 0,
      'Empty image serializations are empty Buffers'
    );
  }
  check(
    image.toDataURL() === 'data:image/png;base64,' &&
      image.getScaleFactors().length === 0 &&
      image.getAspectRatio() === 1,
    'Observed Electron empty-image values'
  );
  check(
    image.crop({ x: 0, y: 0, width: 1, height: 1 }).isEmpty() &&
      image.resize({ width: 10, height: 10 }).isEmpty(),
    'Empty transformations remain empty'
  );
  image.setTemplateImage(true);
  check(image.isTemplateImage() === false, 'Empty image cannot become a template bitmap');
}
sources[0].name = 'caller mutation';
const screens = await first.api.desktopCapturer.getSources({
  types: ['screen'],
  thumbnailSize: { width: 0, height: 150 }
});
const windows = await first.api.desktopCapturer.getSources({
  types: ['window'],
  thumbnailSize: { width: 150, height: 0 }
});
check(
  screens.length === 1 &&
    screens[0].name === records[0].name &&
    windows.length === 1 &&
    windows[0].id === records[1].id,
  'Type filter, either zero thumbnail dimension and isolated snapshots'
);
console.log('PASS controlled metadata projection, filtering and observed empty-image behavior');

let invalidRecordIndex = 0;
for (const bad of [
  null,
  {},
  [{ ...records[0], id: 'screen:0007:0' }],
  [{ ...records[0], id: 'screen:7:1' }],
  [{ ...records[1], id: 'window:42:2' }],
  [{ ...records[1], id: 'window:42:1' }],
  [{ ...records[1], id: 'window:0:0' }],
  [{ ...records[0], name: 42 }],
  [{ ...records[0], name: 'x'.repeat(4097) }],
  [{ ...records[0], name: 'ä'.repeat(2049) }],
  [records[0], records[0]],
  Array.from({ length: 300 }, (_value, index) => ({
    id: 'screen:' + (index + 1) + ':0',
    name: 'x'.repeat(4096),
    display_id: String(index + 1)
  })),
  Array.from({ length: 1025 }, (_value, index) => ({
    id: 'screen:' + (index + 1) + ':0',
    name: 'bounded',
    display_id: String(index + 1)
  }))
]) {
  first.state.onStart = (id) => first.result(id, bad);

  try {
    await rejected(() => first.api.desktopCapturer.getSources(valid()), 'ERR_FIREFLY_CAPTURE_DATA');
  } catch (error) {
    throw Error('Malformed metadata case ' + invalidRecordIndex + ': ' + error.message);
  }
  ++invalidRecordIndex;
}

first.state.onStart = (id) =>
  first.failure(id, 'ERR_TEST_CAPTURE_DENIED', 'controlled permission denial');

const denied = await rejected(
  () => first.api.desktopCapturer.getSources(valid()),
  'ERR_TEST_CAPTURE_DENIED'
);
check(denied.message.includes('controlled permission denial'), 'Provider error is explicit');
console.log(
  'PASS malformed provider metadata, canonical IDs, byte/count limits and explicit provider errors'
);

const reentrant = fixture();
reentrant.ready();
let nested;
let entered = false;

reentrant.state.onStart = (id) => {
  if (!entered) {
    entered = true;
    nested = reentrant.api.desktopCapturer.getSources(valid());
  }
  reentrant.result(id);
};

const outer = reentrant.api.desktopCapturer.getSources(valid());
check(
  (await outer).length === 2 && (await nested).length === 2 && reentrant.state.starts.length === 2,
  'Synchronous provider result and reentrant request retain ownership'
);
const getterCounts = { types: 0, size: 0, icons: 0, width: 0, height: 0 };
const getterOptions = {
  get types() {
    ++getterCounts.types;
    return ['screen'];
  },

  get thumbnailSize() {
    ++getterCounts.size;
    return {
      get width() {
        ++getterCounts.width;
        return 0;
      },

      get height() {
        ++getterCounts.height;
        return 0;
      }
    };
  },

  get fetchWindowIcons() {
    ++getterCounts.icons;
    return false;
  }
};
check(
  (await reentrant.api.desktopCapturer.getSources(getterOptions)).length === 1,
  'Getter options keep selected source types'
);
check(
  Object.values(getterCounts).every((value) => value === 1),
  'Option and size fields are read exactly once'
);
const exiting = fixture();
exiting.ready();
const exitOptions = valid();
Object.defineProperty(exitOptions, 'thumbnailSize', {
  enumerable: true,

  get() {
    exiting.api.app.exit(0);
    return { width: 0, height: 0 };
  }
});
await rejected(() => exiting.api.desktopCapturer.getSources(exitOptions), 'ERR_FIREFLY_APP_EXITED');
check(exiting.state.starts.length === 0, 'Option getter shutdown cannot admit a native request');
console.log('PASS synchronous provider completion and request reentry');

const originalSetTimeout = globalThis.setTimeout;
const originalClearTimeout = globalThis.clearTimeout;
const timers = new Map();

globalThis.setTimeout = (callback, duration, ...args) => {
  if (duration !== 30000) {
    return originalSetTimeout(callback, duration, ...args);
  }
  const handle = {
    unref() {
      return this;
    }
  };
  timers.set(handle, callback);
  return handle;
};

globalThis.clearTimeout = (handle) => {
  if (!timers.delete(handle)) {
    originalClearTimeout(handle);
  }
};

try {
  const bounded = fixture();
  bounded.ready();
  const settled = [];
  const pending = Array.from({ length: 8 }, () =>
    bounded.api.desktopCapturer.getSources(valid()).then(
      (value) => {
        settled.push('resolved');
        return value;
      },
      (error) => {
        settled.push('rejected');
        return error;
      }
    )
  );
  check(bounded.state.starts.length === 8, 'Eight provider requests admitted');
  await rejected(
    () => bounded.api.desktopCapturer.getSources(valid()),
    'ERR_FIREFLY_CAPTURE_LIMIT'
  );
  check(bounded.state.starts.length === 8, 'Quota rejects before provider');
  const firstTimer = timers.keys().next().value;
  const expire = timers.get(firstTimer);
  timers.delete(firstTimer);
  expire();
  const timedOut = await pending[0];
  check(
    timedOut instanceof Error && timedOut.code === 'ERR_FIREFLY_CAPTURE_TIMEOUT',
    'Timeout rejects explicitly'
  );
  check(
    bounded.state.cancels.includes(bounded.state.starts[0]),
    'Timeout cancels native result delivery'
  );
  bounded.result(bounded.state.starts[0]);
  check(settled.length === 1, 'Late timeout result cannot settle other requests');
  const replacement = bounded.api.desktopCapturer.getSources(valid()).catch((error) => error);
  check(bounded.state.starts.length === 9, 'Retired timeout releases request quota');
  bounded.api.app.exit(0);
  const stopped = await Promise.all([...pending.slice(1), replacement]);
  check(
    stopped.every((error) => error instanceof Error && error.code === 'ERR_FIREFLY_APP_EXITED'),
    'App shutdown rejects all pending callers'
  );
  check(
    bounded.state.exited[0] === 0 && timers.size === 0,
    'Shutdown cancels all request deadlines'
  );
  await rejected(() => bounded.api.desktopCapturer.getSources(valid()), 'ERR_FIREFLY_APP_EXITED');
  check(bounded.state.starts.length === 9, 'Exited app cannot enter provider');
} finally {
  globalThis.setTimeout = originalSetTimeout;
  globalThis.clearTimeout = originalClearTimeout;
}
console.log('PASS controlled timeout, quota, cancellation, late-result and app-shutdown policy');
console.log('DONE desktop capture policy');
