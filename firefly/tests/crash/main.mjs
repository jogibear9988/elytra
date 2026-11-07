/** Actual public main-only crash capture/recovery; never constructs a BrowserWindow. */
import firefly, { app, crashReporter, BrowserWindow } from 'firefly';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const mode = process.argv[2];

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  try {
    action();
  } catch (error) {
    check(error.code === code, `${code}: ${error}`);
    return;
  }
  throw Error(`Expected ${code}`);
}

try {
  const alias = await import('electron');
  check(
    alias.crashReporter === crashReporter &&
      firefly.crashReporter === crashReporter &&
      require('electron').crashReporter === crashReporter,
    'main crashReporter singleton exports'
  );

  fails(() => crashReporter.getParameters(), 'ERR_FIREFLY_NOT_READY');

  fails(() => crashReporter.start({ uploadToServer: true }), 'ERR_FIREFLY_UNSUPPORTED_API');

  fails(
    () => crashReporter.start({ uploadToServer: false, ignoreSystemCrashHandler: true }),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );

  fails(() => app.setPath('crashDumps', 'relative'), 'ERR_INVALID_ARG_VALUE');

  app.setPath('crashDumps', process.argv[3]);
  check(app.getPath('crashDumps') === process.argv[3], 'actual configured report directory');
  let helperWindow;
  if (mode === 'helper') {
    // Also exercise helpers which already existed when public reporting was configured.
    await app.whenReady();
    helperWindow = new BrowserWindow({ show: false });
    await helperWindow.loadFile('helper.html');
  }
  crashReporter.start({
    uploadToServer: false,
    compress: true,
    productName:
      mode === 'crash'
        ? 'Crash Fixture One 世界🚀'
        : mode === 'helper'
          ? 'Helper Fixture Three'
          : 'Recovery Fixture Two',
    companyName: 'Elytra Test',
    extra: { scope: 'main-only Grüße', overridden: 'main' },
    globalExtra: { overridden: 'global', global: 'all-helpers' }
  });
  check(!crashReporter.getUploadToServer(), 'uploads unexpectedly enabled');
  check(
    crashReporter.getParameters().scope === 'main-only Grüße' &&
      !('global' in crashReporter.getParameters()),
    'main/global annotation separation'
  );
  const params = crashReporter.getParameters();
  params.scope = 'changed';
  check(crashReporter.getParameters().scope === 'main-only Grüße', 'annotation snapshot mutated');

  fails(() => crashReporter.setUploadToServer(true), 'ERR_FIREFLY_UNSUPPORTED_API');

  fails(() => crashReporter.start({ uploadToServer: false }), 'ERR_FIREFLY_CRASH_REPORTER_STARTED');

  fails(() => app.setPath('crashDumps', process.argv[3]), 'ERR_INVALID_ARG_VALUE');

  console.log(`CRASH_REPORTER_READY ${mode}`);
  if (mode === 'crash') {
    // The test parent sends a real SIGABRT after reading the ready marker.
    await new Promise(() => setInterval(() => {}, 1000));
  }
  if (mode === 'helper') {
    const window = helperWindow;
    const pid = window.webContents.getOSProcessId();
    check(pid > 0 && pid !== process.pid, 'actual renderer PID');
    const gone = new Promise((resolve) =>
      window.webContents.once('render-process-gone', (_event, details) => resolve(details))
    );
    console.log(`CRASH_HELPER_READY ${pid}`);
    const details = await gone;
    check(details.reason === 'crashed', 'renderer crash notification');
    window.destroy();
    app.exit(0);
  } else {
    check(mode === 'recover', 'unexpected crash test mode');
    app.exit(0);
  }
} catch (error) {
  console.error(error.stack ?? error);
  app.exit(1);
}
