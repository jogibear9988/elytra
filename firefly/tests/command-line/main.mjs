/** Actual main policy plus one optional hidden renderer for language/painting acceptance. */
import firefly, { app, BrowserWindow } from 'firefly';
import { createRequire } from 'node:module';
import path from 'node:path';
const require = createRequire(import.meta.url);
const original = [...process.argv];

const check = (condition, message) => {
  if (!condition) {
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
  const line = app.commandLine;
  check(
    line === firefly.app.commandLine &&
      line === alias.app.commandLine &&
      line === require('electron').app.commandLine,
    'main singleton'
  );
  check(!line.hasSwitch('electron-compat'), 'host flag leaked into engine snapshot');
  check(
    line.hasSwitch('elytra-one') && line.getSwitchValue('elytra-one') === 'last=value',
    'real argv parsing'
  );
  check(!line.hasSwitch('elytra-after'), 'terminator ignored');

  fails(() => line.hasSwitch('--lang'), 'ERR_INVALID_ARG_VALUE');

  fails(() => line.getSwitchValue(1), 'ERR_INVALID_ARG_TYPE');

  fails(() => line.appendSwitch('enable-features', 'BlinkOnly'), 'ERR_FIREFLY_UNSUPPORTED_API');

  fails(() => line.appendArgument('positional'), 'ERR_FIREFLY_UNSUPPORTED_API');

  check(line.appendSwitch('lang', 'fr-FR') === undefined, 'append return');
  check(line.getSwitchValue('lang') === 'fr-FR', 'appended state');

  fails(() => line.appendSwitch('lang', 'invalid_language_!'), 'ERR_INVALID_ARG_VALUE');

  check(line.getSwitchValue('lang') === 'fr-FR', 'invalid native mutation consumed state');
  check(line.removeSwitch('lang') === undefined && !line.hasSwitch('lang'), 'remove');
  line.appendSwitch('lang', 'de-DE');

  fails(() => line.appendSwitch('force-cpu-painting'), 'ERR_FIREFLY_COMMAND_LINE_TOO_LATE');

  check(line.hasSwitch('force-cpu-painting'), 'native CPU painting policy');
  check(JSON.stringify(process.argv) === JSON.stringify(original), 'process.argv mutated');
  await app.whenReady();

  fails(() => line.appendSwitch('lang', 'en'), 'ERR_FIREFLY_COMMAND_LINE_TOO_LATE');

  if (process.argv.includes('--renderer')) {
    const window = new BrowserWindow({ show: false });
    await window.loadFile(path.join(process.argv[2], 'index.html'));
    let title;
    for (let attempt = 0; attempt < 300; ++attempt) {
      title = window.webContents.getTitle();
      if (title.startsWith('LANGUAGES:')) {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
    check(title === 'LANGUAGES:["de-DE"]', `actual renderer language: ${title}`);
    console.log(
      JSON.stringify({ helper: window.webContents.getOSProcessId(), languages: ['de-DE'] })
    );
    // Let the external test inspect only this owned helper's effective native argv.
    await new Promise((resolve) => setTimeout(resolve, 400));
    window.destroy();
  }
  console.log(
    'PASS actual public command-line snapshots, native effects, rollback and readiness without argv mutation'
  );
  app.exit(0);
} catch (error) {
  console.error(error);
  app.exit(1);
}
