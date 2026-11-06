import { contextBridge } from 'firefly';

contextBridge.exposeInMainWorld('desktop', {
  greeting: 'Published from an isolated preload',

  add: (left, right) => left + right
});
