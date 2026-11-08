/** Firefly-specific serialization, capability-publication and request-budget checks. */
import { contextBridge, ipcRenderer } from 'firefly';

let traps = 0;
let settled = 0;
let phase = 'initial';

// Return fixture-owned diagnostics as plain data: production bridges deliberately
// sanitize source-world exceptions, so throwing here hides which assertion failed.
async function report(operation) {
  try {
    return await operation();
  } catch (error) {
    return { failure: error.message, phase };
  }
}

async function rejects(operation, code) {
  phase = code;
  try {
    await operation();
  } catch (error) {
    if (error.code === code) {
      return;
    }
    throw Error(`Unexpected rejection: ${error.code}`);
  }
  throw Error(`Accepted forbidden IPC input: ${code}`);
}

contextBridge.exposeInMainWorld('desktop', {
  async check() {
    return report(async () => {
      phase = 'raw capability publication';
      contextBridge.exposeInMainWorld('rawIpc', ipcRenderer);
      let denied = false;
      try {
        contextBridge.exposeInMainWorld('rawSend', { send: ipcRenderer.send });
      } catch {
        denied = true;
      }
      if (!denied) {
        throw Error('Published native IPC capability');
      }
      await rejects(() => ipcRenderer.invoke(42), 'ERR_INVALID_ARG_TYPE');
      await rejects(() => ipcRenderer.invoke('a'.repeat(257)), 'ERR_INVALID_ARG_TYPE');
      await rejects(() => ipcRenderer.invoke('ping', ...Array(65)), 'ERR_INVALID_ARG_TYPE');
      const getter = {
        get secret() {
          traps++;
          return 42;
        }
      };
      const proxy = new Proxy(
        {},
        {
          ownKeys() {
            traps++;
            return [];
          }
        }
      );
      for (const value of [
        getter,
        proxy,
        () => 1,
        document,
        new Uint8Array(65537),
        'x'.repeat(65537)
      ]) {
        await rejects(() => ipcRenderer.invoke('ping', value), 'ERR_FIREFLY_IPC_SERIALIZATION');
      }
      if (traps !== 0) {
        throw Error('Serialization ran an input trap');
      }
      await rejects(() => ipcRenderer.invoke('bad-result'), 'ERR_FIREFLY_IPC_REMOTE');
      for (let index = 0; index < 16; index++) {
        ipcRenderer.invoke('held').then(() => {
          settled++;
          document.title = 'OLD_REPLY_RAN';
        });
      }
      await rejects(() => ipcRenderer.invoke('held'), 'ERR_FIREFLY_IPC_LIMIT');
      return true;
    });
  },

  async fresh() {
    return report(async () => {
      phase = 'fresh document ping';
      if (settled !== 0 || (await ipcRenderer.invoke('ping')) !== 'PONG') {
        throw Error('New document inherited old replies');
      }
      return true;
    });
  }
});
