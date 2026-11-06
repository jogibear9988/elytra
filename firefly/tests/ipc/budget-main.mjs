/** The main-process request budget applies across distinct renderer processes. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = path.dirname(fileURLToPath(import.meta.url));
const waiting = [];
let recovering = false;

ipcMain.handle('held', () =>
  recovering ? 'RECOVERED' : new Promise((resolve) => waiting.push(resolve))
);

async function until(predicate, label) {
  for (let index = 0; index < 500; index++) {
    if (predicate()) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Timed out waiting for ' + label);
}

await app.whenReady();
try {
  const windows = [];
  for (let index = 0; index < 5; index++) {
    const window = new BrowserWindow({
      show: false,
      webPreferences: {
        preload: path.join(directory, 'budget.cjs')
      }
    });
    windows.push(window);
    await window.loadFile('budget.html');
    if (index < 4) {
      await until(() => waiting.length === (index + 1) * 16, 'host reservations');
    }
  }
  await until(() => windows[4].webContents.getTitle() === 'BUDGET_REJECTED', 'global limit');
  if (waiting.length !== 64) {
    throw Error('Host admitted more than 64 requests');
  }
  recovering = true;
  for (const resolve of waiting.splice(0)) {
    resolve('DONE');
  }
  await until(
    () =>
      windows.slice(0, 4).every((window) => window.webContents.getTitle() === 'BUDGET_ACCEPTED'),
    'accepted replies'
  );
  await until(() => windows[4].webContents.getTitle() === 'BUDGET_RECOVERED', 'budget recovery');
  console.log(JSON.stringify({ hostLimit: 64, windows: 5, promptRejection: true, recovery: true }));
  for (const window of windows) {
    window.destroy();
  }
  app.quit();
} catch (error) {
  console.error(error.message);
  app.exit(1);
}
