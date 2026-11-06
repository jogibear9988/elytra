/** Authored ESM is bundled into the supported isolated preload payload for this fixture. */
import { contextBridge, ipcRenderer } from 'firefly';
contextBridge.exposeInMainWorld('desktop', {
  roundtrip: (value) => ipcRenderer.invoke('linux:roundtrip', value)
});
