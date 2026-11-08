/** Listen only in the isolated preload and report plain results through fixed channels. */
import { contextBridge, ipcRenderer } from 'firefly';
const limited = true;

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

check(
  ipcRenderer.addListener === ipcRenderer.on && ipcRenderer.off === ipcRenderer.removeListener,
  'listener aliases'
);
let invalid = 0;
if (limited) {
  for (const action of [
    () => ipcRenderer.on(1, () => {}),
    () => ipcRenderer.on('bad', null),
    () => ipcRenderer.off('bad', null)
  ]) {
    try {
      action();
    } catch {
      ++invalid;
    }
  }
  check(invalid === 3, 'listener validation');
}

const temporary = () => {};

if (limited) {
  for (let i = 0; i < 64; ++i) {
    ipcRenderer.on('temporary', temporary);
  }
  let capped = false;
  try {
    ipcRenderer.on('temporary', temporary);
  } catch (error) {
    capped = error.code === 'ERR_FIREFLY_IPC_LIMIT';
  }
  check(capped, 'listener roots not bounded');
}
check(ipcRenderer.removeAllListeners() === ipcRenderer, 'removeAll result');
let blocked = false;
if (limited) {
  blocked = false;
  try {
    contextBridge.exposeInMainWorld('rawListen', { on: ipcRenderer.on });
  } catch {
    blocked = true;
  }
}
let welcome = false;
let stale = false;
const trace = [];

function second(_event, value) {
  trace.push('b' + value);
}

const later = (_event, value) => {
  trace.push('later' + value);
};

check(
  ipcRenderer.on('order', function (event, value) {
    check(
      this === ipcRenderer &&
        event.sender === ipcRenderer &&
        Array.isArray(event.ports) &&
        event.ports.length === 0,
      'listener receiver/event'
    );
    trace.push('a' + value);
    if (value === 0) {
      check(ipcRenderer.removeListener('order', second) === ipcRenderer, 'removeListener result');
      ipcRenderer.addListener('order', later);
    }
  }) === ipcRenderer,
  'on result'
);
ipcRenderer.on('order', second);
check(
  ipcRenderer.once('order', (_event, value) => {
    trace.push('once' + value);
  }) === ipcRenderer,
  'once result'
);

const duplicate = () => {
  trace.push('duplicate');
};

ipcRenderer.on('duplicates', duplicate);

ipcRenderer.on('duplicates', () => {
  trace.push('middle');
});

ipcRenderer.on('duplicates', duplicate);
check(ipcRenderer.off('duplicates', duplicate) === ipcRenderer, 'off result');

ipcRenderer.once('welcome', (_event, payload) => {
  check(
    payload.value === payload.alias &&
      payload.value.self === payload.value &&
      payload.text === 'é\0\ud800',
    'welcome clone'
  );
  payload.value.count++;
  welcome = true;
});

ipcRenderer.on('must-not-cross-documents', () => {
  stale = true;
});

ipcRenderer.once('report', () => {
  ipcRenderer.send('report', { welcome, stale, trace: trace.join(), invalid, blocked });
});

ipcRenderer.on('close', () => {
  ipcRenderer.send('closed-by-listener');
});

ipcRenderer.on('cleanup', () => {
  throw Error('removed channel called');
});

ipcRenderer.removeAllListeners('cleanup');

// Strip the privileged IPC event before calling a document-supplied function.
// Sharing proxy identity across onValue/offValue makes ordinary unsubscription work.
const pageListeners = new Set();
let callbackReport = null;
let callbackRuns = 0;
contextBridge.exposeInMainWorld('pageEvents', {
  onValue(callback) {
    pageListeners.add(callback);
  },

  offValue(callback) {
    pageListeners.delete(callback);
  },

  echo(value) {
    return value;
  }
});

ipcRenderer.on('page-value', (_event, value) => {
  for (const callback of Array.from(pageListeners)) {
    ++callbackRuns;

    callback(value).then((result) => {
      callbackReport = result;
    });
  }
});

ipcRenderer.once('callback-report', () => {
  ipcRenderer.send('callback-report', { callbackReport, callbackRuns });
});
