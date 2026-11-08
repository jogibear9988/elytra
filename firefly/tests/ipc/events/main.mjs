/** Actual public event routing, reload ownership and reentrant window teardown. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
let window;
let early = 0;
let order = [];
let bursts = [];
let frame;
let oldFrame;
const reference = process.argv.includes('--event-reference');

function failed(error) {
  console.error(error.stack ?? error);
  app.exit(1);
}

if (reference) {
  process.on('uncaughtException', failed);
}
const directory = path.dirname(fileURLToPath(import.meta.url));

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

function sender(event) {
  check(
    event.sender === window.webContents &&
      event.senderFrame === window.webContents.mainFrame &&
      event.senderFrame.url.endsWith('#event-source'),
    'native event sender/history'
  );
  check(!event.senderFrame.isDestroyed(), 'active sender destroyed');
  if (frame) {
    check(frame === event.senderFrame, 'frame changed within document');
  }
  frame = event.senderFrame;
}

ipcMain.on('early', (event, value) => {
  check(event.sender === window.webContents && value === 'PRELOAD', 'early preload sender');
  ++early;
  order = [];
  bursts = [];
  frame = null;

  ipcMain.once('message', () => {
    order.push('once');
    return Symbol('ignored return');
  });
});

ipcMain.on('message', function (event, value) {
  check(this === ipcMain, 'event listener receiver');
  sender(event);
  check(
    value.value === value.alias && value.value.self === value.value && value.text === 'é\0\ud800',
    'event clone'
  );
  ++value.value.count;
  order.push('a' + value.n);
});

ipcMain.on('message', (event, value) => {
  sender(event);
  check(value.value.count === 1, 'same cloned data between listeners');
  order.push('b' + value.n);
});

ipcMain.on('burst', (event, index) => {
  sender(event);
  bursts.push(index);
});

ipcMain.handle('boot', () => 'BOOT');

ipcMain.handle('report', (event) => {
  sender(event);
  // once was registered by early after both persistent listeners.
  return { events: order.join() === 'a0,b0,once,a1,b1', early: early > 0, sender: true };
});

ipcMain.handle('burst-report', () => {
  check(
    bursts.every((value, i) => value === i),
    'burst order'
  );
  return bursts.length;
});

ipcMain.on('close', (event) => {
  sender(event);
  window.destroy();
});

async function title(expected) {
  for (let attempt = 0; attempt < 500; ++attempt) {
    const value = window.webContents.getTitle();
    if (value === expected) {
      return;
    }
    if (value.startsWith('ERROR:')) {
      throw Error(value);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('event page timeout: ' + window.webContents.getTitle());
}

app
  .whenReady()
  .then(async () => {
    window = new BrowserWindow({
      show: false,
      webPreferences: { preload: path.join(directory, 'preload.cjs') }
    });
    await window.loadFile(path.join(directory, 'page.html'));
    await title('IPC_EVENTS_OK');
    oldFrame = frame;
    await window.loadFile(path.join(directory, 'page.html'));
    await title('IPC_EVENTS_OK');
    check(
      early === 2 && (reference || (oldFrame.isDestroyed() && frame !== oldFrame)),
      'old event sender retained after reload'
    );
    // A separate action sends a closing event; its native ACK must not access a destroyed window.
    await window.loadFile(path.join(directory, 'close.html')).catch((error) => {
      check(error.code === 'ERR_ABORTED', 'unexpected closing navigation error: ' + error);
    });
    for (let retry = 0; !window.isDestroyed() && retry < 500; ++retry) {
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
    check(window.isDestroyed(), 'close event did not destroy native window');
    console.log(
      reference
        ? 'PASS Electron IPC send: shared early/order/once/cycles/UTF-16/native sender/reload/reentrant close'
        : 'PASS public IPC send: early/order/once/cycles/UTF-16/synchronous errors/native sender/reload/credit/reentrant close'
    );
    app.exit(0);
  })
  .catch(failed);
