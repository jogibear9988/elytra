/** Genuine public offscreen module: verify lifecycle and explicit refusal without OS registrations. */
import { app, globalShortcut } from 'firefly';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

try {
  globalShortcut.register('Control+Alt+Shift+F19', () => {});

  throw Error('Pre-ready registration was admitted');
} catch (error) {
  check(error.code === 'ERR_FIREFLY_NOT_READY', 'Public readiness guard');
}

app.whenReady().then(() => {
  try {
    check(!globalShortcut.isRegistered('Ctrl+F19'), 'Unknown ownership');
    check(globalShortcut.unregister('Ctrl+F19') === undefined, 'Unknown removal');
    check(globalShortcut.registerAll([], () => {}) === true, 'Empty batch');
    let rejected = false;
    try {
      globalShortcut.register('Control+Alt+Shift+F19', () => {});
    } catch (error) {
      rejected =
        error.code ===
        (process.platform === 'darwin'
          ? 'ERR_FIREFLY_UNSUPPORTED_API'
          : 'ERR_FIREFLY_GLOBAL_SHORTCUT_UNAVAILABLE');
    }
    check(rejected, 'Offscreen/unavailable native backend must fail explicitly');
    check(
      !globalShortcut.isRegistered('Control+Alt+Shift+F19'),
      'Failed native admission not owned'
    );
    globalShortcut.unregisterAll();
    console.log(
      'PASS public globalShortcut readiness, ownership and unavailable/offscreen refusal; no OS registration'
    );
    app.exit(0);
  } catch (error) {
    console.error(error);
    app.exit(1);
  }
});
