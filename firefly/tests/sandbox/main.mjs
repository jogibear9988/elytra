/** Actual public global sandbox requirement; one hidden renderer with sandbox:false. */
import firefly, { app, BrowserWindow } from 'firefly';
import { createRequire } from 'node:module';
import path from 'node:path';
import fs from 'node:fs';
const require = createRequire(import.meta.url);
const directory = process.argv[2];
const enabled = process.argv[3] === 'enabled';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

function fails(action, code) {
  let actual;
  try {
    action();
  } catch (error) {
    actual = error.code;
  }
  check(actual === code, `Expected ${code}, got ${actual}`);
}

try {
  const alias = await import('electron');
  check(
    app === firefly.app && app === alias.app && app === require('electron').app,
    'main aliases'
  );
  if (enabled) {
    check(
      app.enableSandbox() === undefined && app.enableSandbox() === undefined,
      'early native enable'
    );
  }
  await app.whenReady();

  fails(() => app.enableSandbox(), 'ERR_FIREFLY_SANDBOX_TOO_LATE');

  if (!enabled) {
    fails(
      () => new BrowserWindow({ webPreferences: { sandbox: false } }),
      'ERR_FIREFLY_UNSUPPORTED_API'
    );

    console.log('PASS default sandbox:false rejection without global enforcement');
    app.exit(0);
  } else {
    const preferences = { sandbox: false, preload: path.join(directory, 'preload.cjs') };
    const window = new BrowserWindow({ show: false, webPreferences: preferences });
    check(preferences.sandbox === false, 'caller preferences modified');
    await window.loadFile(path.join(directory, 'index.html'));
    let title;
    for (let attempt = 0; attempt < 300; ++attempt) {
      title = window.webContents.getTitle();
      if (title === 'SANDBOX_OVERRIDE_OK') {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
    check(title === 'SANDBOX_OVERRIDE_OK', 'actual isolated preload/page proof');
    console.log(JSON.stringify({ sandboxHelper: window.webContents.getOSProcessId() }));
    const ack = path.join(directory, 'inspected');
    for (let attempt = 0; attempt < 1000 && !fs.existsSync(ack); ++attempt) {
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
    check(fs.existsSync(ack), 'external native sandbox inspection missing');
    window.destroy();
    console.log(
      'PASS actual global sandbox override, native helper, preload isolation and late-call refusal'
    );
    app.exit(0);
  }
} catch (error) {
  console.error(error);
  app.exit(1);
}
