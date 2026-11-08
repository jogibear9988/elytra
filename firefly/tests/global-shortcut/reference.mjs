/** Windowless Electron baseline: no OS hotkeys are registered and no key events synthesized. */
import { app, globalShortcut } from 'electron';
if (process.platform === 'darwin') {
  app.setActivationPolicy('prohibited');
}

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

let rejected = false;
try {
  globalShortcut.register('Ctrl+F17', () => {});
} catch {
  rejected = true;
}
check(rejected, 'Registration before readiness is rejected without touching OS registration');

app.whenReady().then(() => {
  try {
    check(globalShortcut.isRegistered('Ctrl+F17') === false, 'Unknown shortcut is not owned');
    check(
      globalShortcut.unregister('Ctrl+F17') === undefined,
      'Unknown shortcut unregister is harmless'
    );
    let invalidCallback = false;
    try {
      globalShortcut.register('Ctrl+F17', null);
    } catch {
      invalidCallback = true;
    }
    check(invalidCallback, 'Invalid callback rejected before OS registration');
    check(globalShortcut.registerAll([], () => {}) === true, 'Empty registration batch succeeds');
    check(globalShortcut.unregisterAll() === undefined, 'Empty owner cleanup returns void');
    console.log(
      'PASS Electron globalShortcut read-only/invalid-admission baseline; no OS hotkeys registered'
    );
    app.exit(0);
  } catch (error) {
    console.error(error);
    app.exit(1);
  }
});
