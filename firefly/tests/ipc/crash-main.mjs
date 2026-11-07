/** Kill only the owned renderer once requests are pending, then verify a new window works. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createReceipt } from './receipt.mjs';

const directory = path.dirname(fileURLToPath(import.meta.url));
const waiting = [];
const receipt = createReceipt();

ipcMain.handle(
  'held',
  (event) =>
    new Promise((resolve) => {
      waiting.push({ event, resolve });
      if (waiting.length === 16) {
        receipt.acknowledge();
      }
    })
);

ipcMain.handle('bad-result', () => ({ unsupported() {} }));

ipcMain.handle('ping', () => 'PONG');

async function title(window, expected) {
  for (let index = 0; index < 500; index++) {
    const current = window.webContents.getTitle();
    if (current === expected) {
      return;
    }
    if (current.startsWith('FAIL:')) {
      throw Error(current);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Missing IPC crash title: ' + window.webContents.getTitle());
}

await app.whenReady();
try {
  const options = {
    show: false,
    webPreferences: { preload: path.join(directory, 'security.cjs') }
  };
  const window = new BrowserWindow(options);

  window.webContents.on('render-process-gone', async (_event, details) => {
    try {
      if (
        details.reason !== 'crashed' ||
        window.webContents.mainFrame !== null ||
        !waiting.every((entry) => entry.event.senderFrame.isDestroyed())
      ) {
        throw Error('Renderer crash retained sender ownership');
      }
      for (const entry of waiting.splice(0)) {
        entry.resolve('late crashed-process reply');
      }
      const replacement = new BrowserWindow(options);
      await replacement.loadFile('fresh.html');
      await title(replacement, 'FRESH_READY');
      console.log(JSON.stringify({ crash: true }));
      replacement.destroy();
      window.destroy();
      app.quit();
    } catch (error) {
      console.error(error.message);
      app.exit(1);
    }
  });

  await window.loadFile('security.html');
  await title(window, 'SECURITY_READY');
  // A renderer-local full budget is not acknowledgement of main-handler delivery.
  await receipt.wait(() => `Requests missing before crash: held=${waiting.length}`);
  if (waiting.length !== 16) {
    throw Error(`Requests were not pending before crash: held=${waiting.length}`);
  }
  console.log('CRASH_READY ' + window.webContents.getOSProcessId());
} catch (error) {
  console.error(error.message);
  app.exit(1);
}
