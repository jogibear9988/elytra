/** Defer every IPC call until the native main-process fixture has inspected mainFrame. */
import { contextBridge, ipcRenderer } from 'firefly';
contextBridge.exposeInMainWorld('frameFixture', {
  identify() {
    return ipcRenderer.invoke('frames:identity');
  }
});
