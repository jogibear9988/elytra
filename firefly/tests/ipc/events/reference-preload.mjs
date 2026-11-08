/** Shared Electron reference for the positive send contract; Firefly limits have separate tests. */
import { contextBridge, ipcRenderer } from 'electron';
ipcRenderer.send('early', 'PRELOAD');
const boot = ipcRenderer.invoke('boot');
contextBridge.exposeInMainWorld('desktop', {
  run: async () => {
    await boot;
    history.replaceState(null, '', '#event-source');
    const value = { count: 0 };
    value.self = value;
    for (let n = 0; n < 2; ++n) {
      if (
        ipcRenderer.send('message', { value, alias: value, text: 'é\0\ud800', n }) !== undefined
      ) {
        throw Error('send return');
      }
    }
    ipcRenderer.send('unhandled');
    ipcRenderer.send('error');
    const report = await ipcRenderer.invoke('report');
    if (value.count || !report.events || !report.early || !report.sender) {
      throw Error('reference event contract');
    }
    return true;
  },

  close: () => {
    ipcRenderer.send('close');
  }
});
