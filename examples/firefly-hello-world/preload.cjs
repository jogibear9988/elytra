const { contextBridge } = require('firefly');

contextBridge.exposeInMainWorld('desktop', {
  greeting: 'Published from an isolated preload',
  add: (left, right) => left + right
});
