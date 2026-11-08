/** One hidden window checks publication semantics and fresh source snapshots on reload. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const directory = path.dirname(fileURLToPath(import.meta.url));
const reference = process.argv.includes('--reference');
let window;
let report;

app.on('window-all-closed', () => {});

ipcMain.handle('getter:report', (event, value) => {
  if (event.sender !== window.webContents) {
    throw Error('Getter report sender');
  }
  report = value;
  return true;
});

app
  .whenReady()
  .then(async () => {
    window = new BrowserWindow({
      show: false,
      webPreferences: { preload: path.join(directory, 'preload.cjs') }
    });
    for (let round = 0; round < 2; round++) {
      report = null;
      await window.loadFile(path.join(directory, 'page.html'));
      for (let count = 0; count < 500 && !report; count++) {
        await new Promise((resolve) => setTimeout(resolve, 10));
      }
      if (
        !report ||
        !report.snapshot ||
        !report.descriptor ||
        !report.frozen ||
        !report.calls ||
        !report.rollback ||
        (!reference && !report.strict) ||
        !report.function ||
        !report.isolation
      ) {
        throw Error('Getter publication: ' + JSON.stringify(report));
      }
    }
    window.destroy();
    console.log(
      'PASS preload getter publication: snapshots/descriptors/freeze/receiver/functions/rollback/isolation/reload'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack ?? error);
    app.exit(1);
  });
