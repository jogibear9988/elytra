/** Actual public locale reads and engine language selection; no window is created. */
import firefly, { app, BrowserWindow } from 'firefly';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let actual;
  try {
    action();
  } catch (error) {
    actual = error.code;
  }
  check(actual === code, `Expected ${code}, got ${actual}`);
}

function system() {
  return { languages: app.getPreferredSystemLanguages(), country: app.getLocaleCountryCode() };
}

try {
  check(!app.isReady(), 'entry already ready');
  check(
    app === firefly.app &&
      app === (await import('electron')).app &&
      app === require('electron').app,
    'locale singleton aliases'
  );

  fails(() => app.getSystemLocale(), 'ERR_FIREFLY_NOT_READY');

  fails(() => app.getLocale(), 'ERR_FIREFLY_NOT_READY');

  const before = system();
  app.getPreferredSystemLanguages().push('private-language-test');
  check(JSON.stringify(system()) === JSON.stringify(before), 'mutable OS preferences');
  if (process.argv[4]) {
    app.setPath('sessionData', process.argv[4]);
  }
  const mode = process.argv[2];
  const expected = process.argv[3];
  if (mode === 'append') {
    app.commandLine.appendSwitch('lang', expected);
  }
  if (mode === 'remove') {
    app.commandLine.appendSwitch('lang', 'fr-FR');
    app.commandLine.removeSwitch('lang');
  }

  app.on('ready', () => {
    check(
      app.getLocale() === expected && app.getSystemLocale().length > 0,
      'ready listener locale'
    );
  });

  await app.whenReady();
  check(app.getLocale() === expected, `Selected ${app.getLocale()}, expected ${expected}`);
  check(JSON.stringify(system()) === JSON.stringify(before), '--lang changed OS preferences');

  fails(() => app.commandLine.appendSwitch('lang', 'en-US'), 'ERR_FIREFLY_COMMAND_LINE_TOO_LATE');

  check(BrowserWindow.getAllWindows().length === 0, 'locale reads constructed a window');
  console.log(
    JSON.stringify({ ...system(), system: app.getSystemLocale(), application: app.getLocale() })
  );
  console.log('PASS actual public locale APIs before/after readiness, native settings and aliases');
  app.exit(0);
} catch (error) {
  console.error(error.stack ?? error);
  app.exit(1);
}
