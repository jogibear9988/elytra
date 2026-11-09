/** Main event policy against the actual factory/EventEmitter, without native windows. */
import createIpc from '../../../core/ipc-main.mjs';
import { EventEmitter } from 'node:events';

const error = (code, message) => Object.assign(Error(message), { code });

const native = { decodeIpc: JSON.parse };
let active = true;
let canonicalUrl = 'app://bundle/first';
const canonicalFrame = Object.freeze({
  get url() {
    return canonicalUrl;
  },

  isDestroyed: () => !active
});
const ipc = createIpc(native, EventEmitter, error, (id, document) =>
  id === 7 && document === '1:2' && active ? canonicalFrame : undefined
);
const window = { id: 7, webContents: {}, isDestroyed: () => !active };
const data = {
  document: '1:2',
  realm: 'preload',
  url: 'app://bundle/first',
  wire: '["message",7]'
};
const order = [];
let frame;

ipc.ipcMain.once('message', (event, value) => {
  frame = event.senderFrame;
  if (event.sender !== window.webContents || value !== 7) {
    throw Error('sender/data');
  }
  order.push('once');
});

const listener = function (event) {
  if (this !== ipc.ipcMain) {
    throw Error('event receiver');
  }
  if (event.senderFrame !== frame) {
    throw Error('frame alias');
  }
  order.push('on');
};

ipc.ipcMain.on('message', listener);
ipc.dispatchEvent(window, data);
canonicalUrl = 'app://bundle/second';
ipc.dispatchEvent(window, { ...data, url: 'app://forged-packet/not-authority' });
if (order.join() !== 'once,on,on' || frame.url !== 'app://bundle/second') {
  throw Error('order/history');
}
ipc.ipcMain.removeListener('message', listener);
ipc.dispatchEvent(window, data);
ipc.dispatchEvent(window, { ...data, wire: '["error"]' });
ipc.dispatchEvent(window, { ...data, wire: '["unknown"]' });
if (order.length !== 3) {
  throw Error('removed listener');
}
const warn = console.warn;
let malformed = 0;

console.warn = () => {
  ++malformed;
};

try {
  for (const wire of ['null', '[]', '[1]', 'not a graph']) {
    ipc.dispatchEvent(window, { ...data, wire });
  }
} finally {
  console.warn = warn;
}
if (malformed !== 4) {
  throw Error('malformed event reached application dispatch');
}
let onceCalls = 0;

ipc.ipcMain.once('reentrant', () => {
  ++onceCalls;
  ipc.dispatchEvent(window, { ...data, wire: '["reentrant"]' });
});

ipc.dispatchEvent(window, { ...data, wire: '["reentrant"]' });
if (onceCalls !== 1) {
  throw Error('once removed too late');
}
let thrown = false;

ipc.ipcMain.on('throws', () => {
  throw Error('listener failure');
});

try {
  ipc.dispatchEvent(window, { ...data, wire: '["throws"]' });
} catch {
  thrown = true;
}
if (!thrown) {
  throw Error('listener exceptions swallowed');
}
ipc.cancel(window);
active = false; // Controlled native document retirement, independent of IPC cancellation.
if (!frame.isDestroyed()) {
  throw Error('native sender not revoked');
}

console.log(
  'PASS main IPC event ordering/once/removal/sender/history/revocation and listener failures without windows'
);
