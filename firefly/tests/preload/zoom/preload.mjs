/** Code-OSS sets zoom during preload configuration, before application DOM work. */
import { contextBridge, ipcRenderer, webFrame } from 'firefly';
webFrame.setZoomLevel(2);
const ready = ipcRenderer.invoke('zoom:config').then((config) => {
  webFrame.setZoomLevel(config.zoomLevel);
});
let denied = false;
try {
  contextBridge.exposeInMainWorld('rawZoom', { set: webFrame.setZoomFactor });
} catch {
  denied = true;
}
if (process.versions.firefly) {
  contextBridge.exposeInMainWorld('zoomModule', webFrame);
}
contextBridge.exposeInMainWorld('zoomFixture', {
  ready: () => ready,

  strict: Boolean(process.versions.firefly),
  initial: webFrame.getZoomLevel(),
  denied,

  factor: () => webFrame.getZoomFactor(),

  level: () => webFrame.getZoomLevel(),

  setFactor: (value) => webFrame.setZoomFactor(value),

  setLevel: (value) => webFrame.setZoomLevel(value),

  report: (value) => ipcRenderer.invoke('zoom:report', value)
});
