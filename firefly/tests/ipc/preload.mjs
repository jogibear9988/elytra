/** Native IPC migration fixture: publish fixed test actions from the isolated preload. */
import { contextBridge, ipcRenderer } from 'firefly';
const boot = ipcRenderer.invoke('boot');
contextBridge.exposeInMainWorld('desktop', {
  run: async () => {
    try {
      if ((await boot) !== 'BOOT') {
        throw Error('Early preload invocation');
      }
      const value = { a: 1 };
      value.self = value;
      const data = { value, alias: value, neg: -0, inf: Infinity, nan: NaN, text: 'é\0\ud800' };
      const reply = await ipcRenderer.invoke('echo', data);
      if (
        value.a !== 1 ||
        reply.value.a !== 2 ||
        reply.value !== reply.alias ||
        reply.value.self !== reply.value ||
        !Object.is(reply.neg, -0) ||
        reply.inf !== Infinity ||
        !Number.isNaN(reply.nan) ||
        reply.text !== data.text ||
        Object.isFrozen(reply)
      ) {
        throw Error('IPC data clone');
      }
      if ((await ipcRenderer.invoke('once', 40)) !== 42) {
        throw Error('One-time result');
      }
      let missing = false;
      let once = false;
      let denied = false;
      let bad = false;
      try {
        await ipcRenderer.invoke('absent');
      } catch {
        missing = true;
      }
      try {
        await ipcRenderer.invoke('once', 0);
      } catch {
        once = true;
      }
      try {
        await ipcRenderer.invoke('denied');
      } catch {
        denied = true;
      }
      try {
        await ipcRenderer.invoke('echo', () => {});
      } catch {
        bad = true;
      }
      if (!missing || !once || !denied || !bad) {
        throw Error('Missing IPC rejection');
      }
      if ((await ipcRenderer.invoke('zero')) !== undefined) {
        throw Error('Undefined reply');
      }
      if ((await ipcRenderer.invoke('測\0', 21)) !== 42) {
        throw Error('UTF-16 channel');
      }
      const parallel = await Promise.all(
        Array.from({ length: 6 }, (_, i) => ipcRenderer.invoke('parallel', i))
      );
      if (parallel.join(',') !== '0,2,4,6,8,10') {
        throw Error('Request/reply identity');
      }
      if ((await ipcRenderer.invoke('owner', { claimedId: -1 })) !== 'OWNER') {
        throw Error('Owner sender');
      }
      return { clone: true, early: true, rejections: true, parallel: true, owner: true };
    } catch (e) {
      document.title = 'IPC_ERROR:SOURCE:' + e.message;
      throw e;
    }
  }
});
