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
let rawReport;
let binaryReport;
const burstReports = new Map();
let capacityReport;
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

ipcMain.on('raw-page-result', (event, data) => {
  check(event.sender === window.webContents, 'raw callback result sender');
  rawReport = data;
});

ipcMain.on('binary:report', (_event, value) => {
  binaryReport = value;
});

ipcMain.on('burst-report', (event, data) => {
  burstReports.set(event.sender, data);
});

ipcMain.on('capacity-report', (_event, data) => {
  capacityReport = data;
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
  // More than sixteen synchronous sends must queue without exposing native
  // in-flight credit. Every send owns its snapshot before caller mutation.
  burstReports.delete(window.webContents);
  const burst = { index: 0, bytes: new Uint8Array(32) };
  for (let index = 0; index < 128; ++index) {
    burst.index = index;
    burst.bytes.fill(index);
    window.webContents.send('burst', index, burst);
    burst.index = -1;
    burst.bytes.fill(255);
  }
  window.webContents.send('burst-report');
  await until(() => burstReports.has(window.webContents), 'burst report missing');
  const burstReport = burstReports.get(window.webContents);
  check(
    burstReport.count === 128 && burstReport.ordered && burstReport.cloned,
    'queued FIFO/snapshot burst: ' + JSON.stringify(burstReport)
  );
  console.log('IPC_BURST_FIFO_CLONES_PASS');

  if (!reference) {
    // Sixteen active messages plus 256 waiting entries form the finite cap.
    // An earlier report may still occupy active credit, hence the bounded range.
    // No owner-thread yield within this loop: the finite serialized backlog
    // must reject synchronously, rather than silently dropping admitted sends.
    capacityReport = null;
    let admitted = 0;
    let capped = false;
    for (let index = 0; index <= 272; ++index) {
      try {
        window.webContents.send('capacity', index);
        ++admitted;
      } catch (error) {
        if (error.code !== 'ERR_FIREFLY_IPC_LIMIT') {
          throw error;
        }
        capped = true;
        break;
      }
    }
    check(
      capped && admitted >= 256 && admitted <= 272,
      'serialized backlog cap/admission: ' + admitted
    );
    // Once genuine delivery ACKs free space, a new send must be admitted.
    for (let count = 0; count < 500; ++count) {
      await new Promise((resolve) => setTimeout(resolve, 10));
      try {
        window.webContents.send('capacity-report');
        break;
      } catch (error) {
        if (error.code !== 'ERR_FIREFLY_IPC_LIMIT' || count === 499) {
          throw error;
        }
      }
    }
    await until(() => capacityReport, 'backlog credit recovery missing');
    check(
      capacityReport.count === admitted && capacityReport.ordered,
      'overflow dropped/reordered admitted messages: ' + JSON.stringify(capacityReport)
    );
    console.log('IPC_BACKLOG_LIMIT_RECOVERY_PASS');
  }
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
  rawReport = null;
  window.webContents.send('raw-page-event', { n: 17 });
  window.webContents.send('raw-page-event', { n: 99 });
  await until(() => rawReport, 'raw event callback report missing');
  check(
    rawReport.calls === 1 &&
      rawReport.n === 17 &&
      rawReport.sender &&
      rawReport.ports &&
      rawReport.direct &&
      rawReport.result &&
      rawReport.asyncResult,
    'private module crossed document callback: ' + JSON.stringify(rawReport)
  );
  binaryReport = null;
  window.webContents.send('binary:main');
  await until(() => binaryReport, 'binary report missing');
  check(
    binaryReport.fromMain &&
      binaryReport.geometry &&
      binaryReport.copies &&
      binaryReport.tail &&
      binaryReport.mutable &&
      binaryReport.capped,
    'binary context bridge: ' + JSON.stringify(binaryReport)
  );
  check(value.count === 0, 'renderer mutation reached main');
}

async function crossWindowBurst() {
  // Keep the primary window identity intact for its existing sender assertions.
  const peers = [0, 1].map(
    () =>
      new BrowserWindow({
        show: false,
        webPreferences: { preload: path.join(directory, 'preload.cjs') }
      })
  );
  try {
    await Promise.all(peers.map((peer) => peer.loadFile(path.join(directory, 'page.html'))));
    const snapshot = { index: 0, bytes: new Uint8Array(32) };
    // Interleave both documents in one main-thread turn; neither may starve.
    for (let index = 0; index < 128; ++index) {
      for (const peer of peers) {
        snapshot.index = index;
        snapshot.bytes.fill(index);
        peer.webContents.send('burst', index, snapshot);
        snapshot.index = -1;
        snapshot.bytes.fill(255);
      }
    }
    for (const peer of peers) {
      peer.webContents.send('burst-report');
    }
    await until(
      () => peers.every((peer) => burstReports.has(peer.webContents)),
      'cross-window burst did not drain both documents'
    );
    for (const peer of peers) {
      const result = burstReports.get(peer.webContents);
      check(
        result.count === 128 && result.ordered && result.cloned,
        'cross-window FIFO/snapshot: ' + JSON.stringify(result)
      );
      burstReports.delete(peer.webContents);
    }
    console.log('IPC_CROSS_WINDOW_BURST_PASS');
  } finally {
    for (const peer of peers) {
      if (!peer.isDestroyed()) {
        peer.destroy();
      }
    }
  }
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
    await crossWindowBurst();
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
    if (process.argv.includes('--binding-probe')) {
      const other = new BrowserWindow({
        show: false,
        webPreferences: { preload: path.join(directory, 'preload.cjs') }
      });
      await other.loadFile(path.join(directory, 'page.html'));
      // Keep both real helpers alive while the native observer checks their OS parents.
      await new Promise((resolve) => setTimeout(resolve, 500));
      other.destroy();
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
