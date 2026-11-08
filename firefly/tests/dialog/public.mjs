/** Exact public facade under offscreen: explicit refusal is not native UI acceptance. */
import { app, dialog } from 'firefly';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

async function rejected(action, code) {
  let caught;
  try {
    await action();
  } catch (error) {
    caught = error;
  }
  check(caught?.code === code, `Expected ${code}, got ${caught?.code}`);
}

await rejected(() => dialog.showMessageBox({ message: 'Before ready' }), 'ERR_FIREFLY_NOT_READY');
await rejected(() => dialog.showErrorBox('Startup', 'Before ready'), 'ERR_FIREFLY_UNSUPPORTED_API');

app.whenReady().then(async () => {
  try {
    for (const action of [
      () => dialog.showMessageBox({ message: 'No UI' }),
      () => dialog.showMessageBoxSync({ message: 'No UI' }),
      () => dialog.showOpenDialog({}),
      () => dialog.showSaveDialog({})
    ]) {
      await rejected(action, 'ERR_FIREFLY_UNSUPPORTED_API');
    }
    await rejected(
      () => dialog.showOpenDialog({ securityScopedBookmarks: true }),
      'ERR_FIREFLY_UNSUPPORTED_API'
    );
    console.log(
      'PASS public dialog readiness/startup-error path and explicit offscreen refusal; no OS dialogs shown'
    );
    app.exit(0);
  } catch (error) {
    console.error(error);
    app.exit(1);
  }
});
