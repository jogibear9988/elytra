/** The maintained CodeOSS handoff: transfer genuine ports from preload to principal. */
import { contextBridge, ipcRenderer } from 'firefly';

ipcRenderer.on('handoff', (event, nonce) => window.postMessage(nonce, '*', event.ports));

contextBridge.exposeInMainWorld('rendererPorts', {
  ready() {
    ipcRenderer.send('ports:ready');
  },

  report(value) {
    ipcRenderer.send('ports:report', value);
  }
});
