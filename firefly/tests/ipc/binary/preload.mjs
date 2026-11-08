/** Fixed channels only; the first invoke is queued before a native handle can arrive. */
import { contextBridge, ipcRenderer } from 'firefly';
const early = ipcRenderer.invoke('binary:early', new Uint8Array([0, 128, 255]));
contextBridge.exposeInMainWorld('binary', {
  earlyBinary() {
    return early;
  },

  invokeBinary(value) {
    return ipcRenderer.invoke('binary:echo', value);
  },

  sendBinary(value) {
    ipcRenderer.send('binary:from-page', value);
  },

  binaryFromMain(callback) {
    ipcRenderer.once('binary:from-main', callback);
  },

  binaryFromPage(callback) {
    ipcRenderer.once('binary:to-page', callback);
  },

  processBinaryResult(value) {
    ipcRenderer.send('binary:process-result', value);
  }
});
