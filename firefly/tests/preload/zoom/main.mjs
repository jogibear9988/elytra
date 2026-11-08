/** Exercise actual viewport/DPR changes and independent same-origin Firefly windows. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const directory = path.dirname(fileURLToPath(import.meta.url));
const reference = process.argv.includes('--reference');
let window;
let report;
let independent;
let baselineTitle;

app.on('window-all-closed', () => {});

ipcMain.handle('zoom:config', () => ({ zoomLevel: 2 }));

ipcMain.handle('zoom:report', (event, value) => {
  if (event.sender !== window.webContents) {
    throw Error('Zoom sender');
  }
  report = value;
  return true;
});

app
  .whenReady()
  .then(async () => {
    if (!reference) {
      independent = new BrowserWindow({ show: false });
      await independent.loadFile(path.join(directory, 'independent.html'));
      baselineTitle = independent.getTitle();
    }
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
      if (!report?.ok || (!reference && (!report.denied || !report.redacted))) {
        throw Error('Zoom report: ' + JSON.stringify(report) + ' title=' + window.getTitle());
      }
      if (independent && independent.getTitle() !== baselineTitle) {
        throw Error('Independent window zoom changed');
      }
    }
    window.destroy();
    independent?.destroy();
    console.log(
      'PASS webFrame zoom: early preload/native viewport/DPR/factor/level/invalid/isolation/reload'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack ?? error);
    app.exit(1);
  });
