/** One hidden offscreen batch: genuine DOM handoff and utility-to-renderer relay. */
import { app, BrowserWindow, ipcMain, MessageChannelMain, utilityProcess } from 'firefly';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
let window;
let child;
let ended = false;
const reports = [];
const timer = setTimeout(() => fail(Error('Renderer MessagePort acceptance timed out')), 25000);

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fail(error) {
  if (ended) {
    return;
  }
  ended = true;
  clearTimeout(timer);
  console.error(error.stack || error.message);
  child?.kill();
  window?.destroy();
  app.exit(1);
}

async function until(predicate) {
  for (let attempt = 0; attempt < 1800; attempt++) {
    if (predicate()) {
      return;
    }
    if (ended) {
      throw Error('Acceptance already failed');
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Renderer MessagePort condition did not settle');
}

app
  .whenReady()
  .then(async () => {
    let ready = false;

    ipcMain.on('ports:ready', (event) => {
      check(event.sender === window.webContents, 'Ready belongs to actual owned WebContents');
      ready = true;
    });

    ipcMain.on('ports:report', (event, value) => {
      check(event.sender === window.webContents, 'Report belongs to actual owned WebContents');
      if (value.kind === 'failure') {
        fail(Error(value.message));
      } else {
        reports.push(value);
      }
    });

    window = new BrowserWindow({
      show: false,
      webPreferences: {
        preload: path.join(import.meta.dirname, 'preload.cjs')
      }
    });
    window.loadFile(path.join(import.meta.dirname, 'page.html'));
    await until(() => ready);
    const direct = new MessageChannelMain();
    let received = 0;
    let closed = false;

    direct.port1.on('message', ({ data }) => {
      try {
        if (data.kind === 'queued-echo') {
          check(received === 0 && data.phase === 'direct', 'Pre-transfer queue retained');
        } else {
          check(
            data.kind === 'echo' && data.phase === 'direct' && data.index === received - 1,
            'Main/DOM binary FIFO'
          );
          check(
            data.bytes instanceof Uint8Array &&
              data.bytes.byteLength === 4096 &&
              data.bytes.every((value, offset) => value === ((offset * 13 + data.index) & 255)),
            'Main/DOM independent binary bytes'
          );
        }
        received++;
      } catch (error) {
        fail(error);
      }
    });

    direct.port1.once('close', () => {
      try {
        check(received === 33, 'All 33 accepted echoes precede DOM peer close');
        closed = true;
      } catch (error) {
        fail(error);
      }
    });

    direct.port1.start();
    direct.port1.postMessage({ kind: 'queued' });
    window.webContents.postMessage('handoff', 'direct', [direct.port2]);
    direct.port2.postMessage({ kind: 'detached-must-not-arrive' });
    direct.port2.close();
    for (let index = 0; index < 32; index++) {
      const bytes = new Uint8Array(4096);
      for (let offset = 0; offset < bytes.length; offset++) {
        bytes[offset] = (offset * 13 + index) & 255;
      }
      direct.port1.postMessage({ kind: 'binary', index, bytes });
      bytes.fill(77);
    }
    await until(() => closed && reports.some((value) => value.phase === 'direct'));
    console.log(
      'PASS renderer ports: real principal DOM pair, delayed start, 32 binary FIFO snapshots and ordered close'
    );

    child = utilityProcess.fork(path.join(import.meta.dirname, 'child.mjs'), [], { stdio: 'pipe' });
    child.on('error', fail);
    let stderr = '';
    child.stderr?.on('data', (bytes) => {
      stderr += bytes.toString();
    });
    const notices = [];

    child.on('message', (value) => notices.push(value));

    let exitCode;

    child.once('exit', (code) => {
      exitCode = code;
    });

    const relay = new MessageChannelMain();
    child.postMessage('attach', [relay.port2]);
    await until(() => notices.some((value) => value.kind === 'attached'));
    // Its peer is already native-backed by the utility process. This exercises
    // the actual relay required by CodeOSS, rather than a second local pair.
    window.webContents.postMessage('handoff', 'relay', [relay.port1]);
    relay.port1.postMessage({ kind: 'detached-must-not-arrive' });
    relay.port1.close();
    await until(
      () =>
        notices.some((value) => value.kind === 'relay-complete') &&
        reports.some((value) => value.phase === 'relay')
    );
    const identity = notices.find((value) => value.kind === 'relay-complete');
    check(
      identity.count === 33 &&
        identity.pid === child.pid &&
        identity.pid !== process.pid &&
        identity.mite,
      'Genuine utility child owns relayed peer'
    );
    child.postMessage('exit');
    await until(() => exitCode !== undefined);
    check(exitCode === 0 && child.pid === undefined && !stderr, 'Utility exits cleanly: ' + stderr);
    console.log(
      'PASS renderer ports: real utility/native DOM relay, binary ownership, close and PID retirement'
    );
    ended = true;
    clearTimeout(timer);
    window.destroy();
    app.exit(0);
  })
  .catch(fail);
