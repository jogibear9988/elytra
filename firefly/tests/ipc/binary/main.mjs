/** One hidden window covers real early/bidirectional binary IPC and fresh document epochs. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const directory = path.dirname(fileURLToPath(import.meta.url));
const reference = process.argv.includes('--reference');
let window;
let report;
let oldFrame;

const check = (ok, message) => {
  if (!ok) {
    throw Error(message);
  }
};

app.on('window-all-closed', () => {});

ipcMain.handle('binary:early', (event, value) => {
  check(
    event.sender === window.webContents && value instanceof Uint8Array && value[1] === 128,
    'early sender/type'
  );
  oldFrame ??= event.senderFrame;
  return value;
});

ipcMain.handle('binary:echo', (event, value) => {
  check(event.sender === window.webContents, 'invoke sender');
  return Promise.resolve(value);
});

ipcMain.on('binary:from-page', (event, value) => {
  check(event.sender === window.webContents && value.view[0] === 128, 'send snapshot/sender');
  event.sender.send('binary:to-page', value);
});

ipcMain.on('binary:process-result', (_event, value) => {
  report = value;
});

const until = async (predicate) => {
  for (let i = 0; i < 500; i++) {
    if (predicate()) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Binary IPC report missing');
};

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
      const value = Buffer.from([255, 128, 0]);
      window.webContents.send('binary:from-main', value);
      await until(() => report);
      check(
        report.early && report.main && report.invoke && report.send && value[0] === 255,
        'Binary IPC: ' + JSON.stringify(report)
      );
      if (round && !reference) {
        check(oldFrame.isDestroyed(), 'old binary sender survived reload');
      }
    }
    window.destroy();
    console.log(
      'PASS public raw binary IPC: early invoke, async replies, Buffer normalization, 64KiB views/aliases/offsets, bidirectional sends and reload'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack ?? error);
    app.exit(1);
  });
