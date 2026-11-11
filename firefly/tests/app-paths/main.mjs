/** Public paths and real persistent renderer state under the selected session profile. */
import { app, BrowserWindow, nativeTheme } from 'firefly';
import path from 'node:path';
import fs from 'node:fs';
const [root, stage, origin] = process.argv.slice(2);

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
  const user = path.join(root, 'application');
  const session = path.join(root, 'session');
  check(app.getAppPath() === import.meta.dirname, 'actual application directory');
  check(fs.statSync(app.getPath('exe')).isFile(), 'native executable');
  check(
    app.setPath('userData', user) === undefined && app.getPath('sessionData') === user,
    'session follows user data'
  );

  fails(() => app.setPath('userData', path.join(root, 'missing')), 'ERR_INVALID_ARG_VALUE');

  check(app.getPath('userData') === user, 'failed setter changed data root');
  app.setPath('sessionData', session);
  check(app.getPath('sessionData') === session, 'independent session data');
  app.setAppLogsPath(path.join(root, 'logs/nested'));
  check(fs.statSync(app.getPath('logs')).isDirectory(), 'real log path');
  app.commandLine.appendSwitch('lang', 'de-DE');
  const themeSupported = process.platform === 'darwin';
  if (themeSupported) {
    nativeTheme.themeSource = 'dark';
  } else {
    // Native appearance integration is currently verified only on Cocoa.
    fails(() => {
      nativeTheme.themeSource = 'dark';
    }, 'ERR_FIREFLY_UNSUPPORTED_API');

    fails(() => nativeTheme.themeSource, 'ERR_FIREFLY_UNSUPPORTED_API');

    console.log('PASS native theme explicitly unsupported; profile checks retained');
  }
  app.enableSandbox();
  fs.writeFileSync(path.join(app.getPath('userData'), 'app-setting.txt'), 'persistent app file');
  await app.whenReady();

  fails(() => app.setPath('userData', user), 'ERR_FIREFLY_PATHS_TOO_LATE');

  fails(() => app.setPath('sessionData', session), 'ERR_FIREFLY_PATHS_TOO_LATE');

  check(app.getPath('sessionData') === session, 'profile selection changed at readiness');
  const window = new BrowserWindow({ show: false, webPreferences: { sandbox: false } });
  await window.loadURL(
    `${origin}/?stage=${stage}&theme=${themeSupported ? 'dark' : 'unsupported'}`
  );
  let title;
  for (let attempt = 0; attempt < 500; ++attempt) {
    title = window.webContents.getTitle();
    if (title.startsWith('PERSIST_')) {
      break;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  check(title === 'PERSIST_OK', `actual persistent document: ${title}`);
  console.log(
    JSON.stringify({ pathsHelper: window.webContents.getOSProcessId(), user, session, stage })
  );
  const ack = path.join(root, `inspected-${stage}`);
  for (let attempt = 0; attempt < 1000 && !fs.existsSync(ack); ++attempt) {
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  check(fs.existsSync(ack), 'external native/profile inspection missing');
  window.destroy();
  console.log(
    `PASS actual ${stage} userData/sessionData profile, cookies/storage, early language, platform theme contract and native sandbox`
  );
  app.quit();
} catch (error) {
  console.error(error);
  app.exit(1);
}
