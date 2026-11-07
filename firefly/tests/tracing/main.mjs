/** Actual public host capture/file output; uses native main-loop work, no BrowserWindow. */
import firefly, { app, contentTracing } from 'firefly';
import { createRequire } from 'node:module';
import path from 'node:path';
const require = createRequire(import.meta.url);
const output = process.argv[2];

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

async function rejects(action, expected) {
  let code;
  try {
    await action();
  } catch (error) {
    code = error.code;
  }
  check(code === expected, `Expected ${expected}, got ${code}`);
}

try {
  const alias = await import('electron');
  check(
    firefly.contentTracing === contentTracing &&
      alias.contentTracing === contentTracing &&
      require('electron').contentTracing === contentTracing,
    'public tracing singleton'
  );
  await rejects(() => contentTracing.getCategories(), 'ERR_FIREFLY_NOT_READY');
  await app.whenReady();
  const categories = await contentTracing.getCategories();
  check(categories.length === 3 && categories.includes('firefly.main'), 'native categories');
  await rejects(
    () => contentTracing.startRecording({ traceOptions: 'record-until-full,enable-sampling' }),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );
  await contentTracing.startRecording({ included_categories: ['firefly.main'] });
  await new Promise((resolve) => setTimeout(resolve, 35));
  let usage;
  // The first Qt pump can start late during native application initialization.
  // The currently executing pump is not complete until this JS turn yields.
  for (let attempt = 0; attempt < 30; ++attempt) {
    usage = await contentTracing.getTraceBufferUsage();
    if (usage.value > 0) {
      break;
    }
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
  check(
    usage.value > 0 && usage.value <= 1 && usage.percentage === usage.value * 100,
    'native buffer usage'
  );
  // A failed write must leave a recoverable snapshot, not consume the recording.
  await rejects(
    () => contentTracing.stopRecording(path.join(output, 'missing', 'trace.json')),
    'ENOENT'
  );
  await rejects(() => contentTracing.startRecording({}), 'ERR_FIREFLY_TRACE_BUSY');
  const explicit = await contentTracing.stopRecording(path.join(output, 'explicit.json'));
  await contentTracing.startRecording({ categoryFilter: '*,-firefly.main' });
  await new Promise((resolve) => setTimeout(resolve, 15));
  const temporary = await contentTracing.stopRecording();
  console.log(JSON.stringify({ categories, explicit, temporary, usage }));
  console.log(
    'PASS actual public main tracing, async output/retry, aliases and filtered capture without a window'
  );
  app.exit(0);
} catch (error) {
  console.error(error);
  app.exit(1);
}
