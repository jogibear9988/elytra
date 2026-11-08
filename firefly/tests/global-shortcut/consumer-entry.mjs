/** Actual Firefly bootstrap supplies genuine Electron imports; registry seam is controlled policy only. */
import { app } from 'firefly';
import createGlobalShortcuts from '../../core/global-shortcut/index.mjs';
import exerciseConsumer from './consumer.mjs';

const failure = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
};

app.whenReady().then(async () => {
  const deadline = setTimeout(() => {
    console.error('Global keybindings consumer timed out');
    app.exit(1);
  }, 10000);
  let component;
  try {
    const registrations = [];
    const owned = new Set();
    let mode = 0;
    // This controlled registry is injected at the consumer's explicit constructor seam.
    // Actual native Host worker/GC/ownership tests are a separate maintained gate.
    const native = {
      globalShortcutRegister(token) {
        if (mode === 1) {
          mode = 0;
          return false;
        }
        registrations.push(token);
        owned.add(token);
        return true;
      },

      globalShortcutUnregister(token) {
        owned.delete(token);
      },

      globalShortcutShutdown() {
        owned.clear();
      },

      count: () => registrations.length,

      control(value) {
        mode = value;
      },

      emit(index) {
        component.dispatch({ token: registrations[index] });
      }
    };
    component = createGlobalShortcuts(
      native,
      () => app.isReady(),
      () => false,
      failure,
      unsupported,
      process.platform
    );
    const { GlobalKeybindingsMainService } = await import(process.env.ELYTRA_GLOBAL_CONSUMER);
    await exerciseConsumer(GlobalKeybindingsMainService, component.globalShortcut, native);
    if (owned.size) {
      throw Error('Consumer leaked controlled registry ownership');
    }
    component.stop();
    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    component?.stop();
    clearTimeout(deadline);
    console.error(error);
    app.exit(1);
  }
});
