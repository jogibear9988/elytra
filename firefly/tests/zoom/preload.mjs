/** Narrow measurements and commands; no raw webFrame function crosses the isolated-world boundary. */
import { contextBridge, ipcRenderer, webFrame } from 'firefly';

contextBridge.exposeInMainWorld('publicZoom', {
  config: () => ipcRenderer.invoke('public-zoom:config'),

  command: (command) => ipcRenderer.invoke('public-zoom:command', command),

  factor: () => webFrame.getZoomFactor(),

  level: () => webFrame.getZoomLevel(),

  setFactor: (value) => webFrame.setZoomFactor(value),

  report: (value) => ipcRenderer.invoke('public-zoom:report', value),

  independent: (value) => ipcRenderer.invoke('public-zoom:independent', value)
});
