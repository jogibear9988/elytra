/** Actual Code-OSS platform/process modules must recognize Mite without fabricated identity. */
import { pathToFileURL } from 'node:url';
const root = pathToFileURL(process.argv[2] + '/').href;
const platform = await import(new URL('src/vs/base/common/platform.js', root).href);
const safe = await import(new URL('src/vs/base/common/process.js', root).href);
const native = process.argv[3] === 'native';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

if (native) {
  check(
    platform.isNative && !platform.isWeb && !platform.isElectron,
    'Native runtime classification'
  );
  check(platform.isWindows === (process.platform === 'win32'), 'Windows classification');
  check(platform.isMacintosh === (process.platform === 'darwin'), 'macOS classification');
  check(platform.isLinux === (process.platform === 'linux'), 'Linux classification');
  check(
    safe.platform === process.platform && safe.arch === process.arch,
    'Actual native platform/arch'
  );
  check(
    safe.env === process.env && safe.cwd() === process.env.VSCODE_CWD,
    'Native environment/cwd'
  );
  check(platform.language === 'fr' && platform.locale === 'fr-fr', 'Native language configuration');
  check(platform.platformLocale === 'de-de', 'Native OS locale configuration');
  check(platform.regionalLocale === 'de-at', 'Native regional locale configuration');
  if (process.release.name === 'mite') {
    check(typeof process.versions.mite === 'string', 'Actual Mite version');
    check(
      process.versions.node === undefined && process.versions.electron === undefined,
      'No fake engine identity'
    );
  }
  let callback = false;

  platform.setTimeout0(() => {
    callback = true;
  });

  await new Promise((resolve) => setTimeout(resolve, 20));
  check(callback, 'Actual Code-OSS scheduler uses native timer');
  console.log(
    'PASS actual Code-OSS native platform/process/locales/timer with truthful runtime identity'
  );
} else {
  check(
    !platform.isNative && safe.arch === undefined && safe.cwd() === '/',
    'Unadapted Mite falls back incorrectly'
  );
  console.log('PASS unadapted Code-OSS rejects native Mite classification');
}
