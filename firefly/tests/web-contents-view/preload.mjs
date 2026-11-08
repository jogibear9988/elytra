/** Fixed-capability preload for real renderer identity, viewport sampling and binary IPC. */
import { contextBridge, ipcRenderer } from 'firefly';

const identity = process.argv.find((value) => value.startsWith('--view-fixture='))?.split('=')[1];
contextBridge.exposeInMainWorld('viewFixture', {
  identity,
  pid: process.pid,
  secure:
    process.sandboxed === true && process.contextIsolated === true && process.isMainFrame === true,

  ready() {
    return ipcRenderer.invoke('view:ready', identity);
  },

  onSample(callback) {
    ipcRenderer.on('view:sample', (_event, request) => callback(request));
  },

  echo(value) {
    return ipcRenderer.invoke('view:echo', value);
  },

  report(value) {
    return ipcRenderer.invoke('view:report', value);
  }
});
