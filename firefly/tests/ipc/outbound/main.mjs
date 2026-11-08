/** Send before any renderer application message, then verify bounded delivery and reload epochs. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
const reference = process.argv.includes('--reference');
const directory = path.dirname(fileURLToPath(import.meta.url));
const profile = path.join(directory, 'session');
fs.mkdirSync(profile, { recursive: true });
app.setPath('userData', profile);
const pauseFile = process.argv.find((value) => value.startsWith('--pause-file='))?.slice(13);

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

// Keep the reference alive until post-destruction assertions complete.
app.on('window-all-closed', () => {});

let window;
let report;
let callbackReport;
let frame;
const value = { count: 0 };
value.self = value;
const payload = { value, alias: value, text: 'é\0\ud800' };

function failed(error) {
  console.error(error.stack ?? error);
  app.exit(1);
}

if (reference) {
  process.on('uncaughtException', failed);
}

ipcMain.on('report', (event, data) => {
  check(
    event.sender === window.webContents && event.senderFrame === window.webContents.mainFrame,
    'report sender'
  );
  frame = event.senderFrame;
  report = data;
});

ipcMain.on('callback-report', (_event, data) => {
  callbackReport = data;
});

ipcMain.on('closed-by-listener', (event) => {
  check(event.sender === window.webContents, 'close sender');
  window.destroy();
});

async function until(predicate, message) {
  for (let count = 0; count < 500; ++count) {
    if (predicate()) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error(message);
}

async function round() {
  report = null;
  await window.loadFile(path.join(directory, 'page.html'));
  // No invoke/send from the preload was needed to bind this document's receiver.
  check(window.webContents.send('welcome', payload) === undefined, 'webContents.send return');
  window.webContents.send('order', 0);
  window.webContents.send('order', 1);
  window.webContents.send('duplicates');
  window.webContents.send('cleanup');
  for (let i = 5; i < 16; ++i) {
    window.webContents.send('ignored');
  }
  if (!reference) {
    let capped = false;
    try {
      window.webContents.send('ignored');
    } catch (error) {
      capped = error.code === 'ERR_FIREFLY_IPC_LIMIT';
    }
    check(capped, 'native outgoing queue not bounded');
  }
  // Retry only until private ACK credit is available; send itself stays synchronous.
  for (let count = 0; count < 500; ++count) {
    await new Promise((resolve) => setTimeout(resolve, 10));
    try {
      window.webContents.send('report');
      break;
    } catch (error) {
      if (error.code !== 'ERR_FIREFLY_IPC_LIMIT' || count === 499) {
        throw error;
      }
    }
  }
  await until(() => report, 'renderer event report missing');
  check(
    report.welcome &&
      !report.stale &&
      (reference || (report.invalid === 3 && report.blocked)) &&
      report.trace === 'a0,b0,once0,a1,later1,duplicate,middle',
    'listener order/removal/snapshot: ' + JSON.stringify(report)
  );
  callbackReport = null;
  window.webContents.send('page-value', { n: 4 });
  if (!reference) {
    window.webContents.send('page-value', { n: 10 });
  }
  window.webContents.send('callback-report');
  await until(() => callbackReport, 'page callback report missing');
  const cb = callbackReport.callbackReport;
  check(
    callbackReport.callbackRuns === 1 &&
      cb &&
      cb.calls === 1 &&
      cb.n === 5 &&
      (reference || cb.frozen) &&
      cb.realm &&
      cb.page,
    'page callback identity/reentry/Promise: ' + JSON.stringify(callbackReport)
  );
  check(value.count === 0, 'renderer mutation reached main');
}

app
  .whenReady()
  .then(async () => {
    window = new BrowserWindow({
      show: false,
      webPreferences: { preload: path.join(directory, 'preload.cjs') }
    });
    if (!reference) {
      let invalid = 0;
      for (const send of [
        () => window.webContents.send(1),
        () => window.webContents.send('bad', () => {})
      ]) {
        try {
          send();
        } catch {
          ++invalid;
        }
      }
      check(invalid === 2, 'outgoing validation');
    }
    await round();
    const old = frame;
    window.webContents.send('must-not-cross-documents');
    await round();
    if (!reference) {
      check(old.isDestroyed() && frame !== old, 'document sender not revoked');
    }
    if (!reference && pauseFile) {
      console.log(JSON.stringify({ pause: window.webContents.getOSProcessId() }));
      await until(() => fs.existsSync(pauseFile), 'test peer was not paused');
      let admitted = 0;
      for (let i = 0; i < 16; ++i) {
        try {
          window.webContents.send('ignored');
          ++admitted;
        } catch (error) {
          if (error.code !== 'ERR_FIREFLY_IPC_LIMIT') {
            throw error;
          }
          break;
        }
      }
      check(admitted > 0, 'no outgoing credit for stalled-peer test');
      await new Promise((resolve) => setTimeout(resolve, 32000));
      let retired = false;
      try {
        window.webContents.send('ignored');
      } catch (error) {
        retired = error.code === 'ERR_FIREFLY_IPC_REVOKED';
      }
      check(retired, 'timeout replenished credit to stalled peer');
      console.log('IPC_TIMEOUT_RETIRED');
      await until(() => fs.existsSync(pauseFile + '.resumed'), 'test peer not resumed');
      await round();
    }
    window.webContents.send('close');
    await until(() => window.isDestroyed(), 'renderer close callback missing');
    console.log(
      reference
        ? 'PASS Electron main-to-renderer event reference: bind/listeners/clones/order/reload/close'
        : 'PASS public main-to-renderer events: automatic bind/listeners/clones/order/limits/revocation/close'
    );
    app.exit(0);
  })
  .catch(failed);
