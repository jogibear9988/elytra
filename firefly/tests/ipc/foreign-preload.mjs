/** Native IPC migration fixture: try the owner-only handler from a different native window. */
import { contextBridge, ipcRenderer } from 'firefly';
contextBridge.exposeInMainWorld('desktop', {
  claim: async () => {
    try {
      await ipcRenderer.invoke('owner', { claimedId: 1 });
      return false;
    } catch {
      return true;
    }
  }
});
