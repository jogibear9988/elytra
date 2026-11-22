/** Publish only fixed window actions from the isolated top-level preload. */
import { contextBridge, ipcRenderer } from 'firefly';

const controls = {};
for (const action of ['minimize', 'maximize', 'unmaximize', 'restore', 'state', 'close']) {
  controls[action] = () => ipcRenderer.invoke('designer:window', action);
}
contextBridge.exposeInMainWorld('designerWindow', controls);

if (process.argv.includes('--designer-smoke')) {
  contextBridge.exposeInMainWorld('designerSmoke', {
    stage: (name) => ipcRenderer.send('designer:smoke-stage', name),

    report: (result) => ipcRenderer.invoke('designer:smoke-result', result)
  });
}
