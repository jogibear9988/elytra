/** Publish fixed application actions, never the raw send capability. */
import { contextBridge, ipcRenderer } from 'firefly';
ipcRenderer.send('early', 'PRELOAD');
const boot = ipcRenderer.invoke('boot');
let rawBlocked = false;
try {
  contextBridge.exposeInMainWorld('rawEventCapability', { send: ipcRenderer.send });
} catch {
  rawBlocked = true;
}

contextBridge.exposeInMainWorld('desktop', {
  run: async () => {
    await boot;
    let traps = 0;
    const hostile = {};
    Object.defineProperty(hostile, 'value', {
      enumerable: true,

      get() {
        ++traps;
        throw Error('getter');
      }
    });
    const proxy = new Proxy(
      {},
      {
        ownKeys() {
          ++traps;
          throw Error('proxy');
        }
      }
    );
    for (const action of [
      () => ipcRenderer.send(1),
      () => ipcRenderer.send('bad', () => {}),
      () => ipcRenderer.send('bad', hostile),
      () => ipcRenderer.send('bad', proxy)
    ]) {
      let threw = false;
      try {
        action();
      } catch {
        threw = true;
      }
      if (!threw) {
        throw Error('send must reject synchronously');
      }
    }
    history.replaceState(null, '', '#event-source');
    const value = { count: 0 };
    value.self = value;
    const first = ipcRenderer.send('message', { value, alias: value, text: 'é\0\ud800', n: 0 });
    const second = ipcRenderer.send('message', { value, alias: value, text: 'é\0\ud800', n: 1 });
    if (first !== undefined || second !== undefined || value.count !== 0 || traps || !rawBlocked) {
      throw Error('send result/isolation');
    }
    ipcRenderer.send('unhandled');
    ipcRenderer.send('error');
    const report = await ipcRenderer.invoke('report');
    if (
      !report.events ||
      !report.early ||
      !report.sender ||
      typeof globalThis.rawEventCapability !== 'undefined'
    ) {
      throw Error('event report');
    }
    return true;
  },

  burst: async () => {
    for (let i = 0; i < 256; ++i) {
      ipcRenderer.send('burst', i);
    }
    let limited = false;
    try {
      ipcRenderer.send('burst', 256);
    } catch (error) {
      limited = error.code === 'ERR_FIREFLY_IPC_LIMIT';
    }
    if (!limited) {
      throw Error('event pending budget');
    }
    for (let retry = 0; retry < 500; ++retry) {
      await new Promise((resolve) => setTimeout(resolve, 10));
      try {
        if ((await ipcRenderer.invoke('burst-report')) !== 256) {
          throw Error('native burst dispatch');
        }
        return true;
      } catch (error) {
        if (error.code !== 'ERR_FIREFLY_IPC_LIMIT') {
          throw error;
        }
      }
    }
    throw Error('event credit did not recover');
  },

  close: () => {
    ipcRenderer.send('close');
  }
});
