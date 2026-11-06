import { contextBridge, ipcRenderer } from 'firefly';

// Constants come from the reviewed sample configuration at build time. Each
// method has one fixed channel/action; neither renderer chooses a channel.
const methods = Object.fromEntries(
  __EXAMPLE_ACTIONS__.map((action) => [
    action,
    (value) => ipcRenderer.invoke(__EXAMPLE_CHANNEL__, action, value)
  ])
);
contextBridge.exposeInMainWorld('desktop', {
  sample: __EXAMPLE_ID__,
  add: (left, right) => left + right,
  ...methods
});
