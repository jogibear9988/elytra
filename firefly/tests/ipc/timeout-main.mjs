/** A never-settling main handler must not leave a renderer request pending forever. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = path.dirname(fileURLToPath(import.meta.url));

ipcMain.handle('never', () => new Promise(() => {}));

ipcMain.handle('ping', () => 'PONG');

await app.whenReady();
try {
  const window = new BrowserWindow({
    show: false,
    webPreferences: {
      preload: path.join(directory, 'timeout.cjs')
    }
  });
  await window.loadFile('timeout.html');
  let complete = false;
  for (let index = 0; index < 3500; index++) {
    const title = window.webContents.getTitle();
    if (title === 'TIMEOUT_OK') {
      complete = true;
      break;
    }
    if (title.startsWith('FAIL:')) {
      throw Error(title);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  if (!complete) {
    throw Error('Missing timeout completion');
  }
  console.log(JSON.stringify({ deadlineSeconds: 30, recovery: true }));
  window.destroy();
  app.quit();
} catch (error) {
  console.error(error.message);
  app.exit(1);
}
