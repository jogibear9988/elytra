/** Verify the actual alpha deadline without test-only timeout overrides. */
import { contextBridge, ipcRenderer } from 'firefly';

contextBridge.exposeInMainWorld('desktop', {
  async deadline() {
    const started = Date.now();
    try {
      await ipcRenderer.invoke('never');
    } catch (error) {
      if (error.code !== 'ERR_FIREFLY_IPC_REMOTE' || !error.message.includes('timed out')) {
        throw error;
      }
      const elapsed = Date.now() - started;
      if (elapsed < 29500 || elapsed > 35000 || (await ipcRenderer.invoke('ping')) !== 'PONG') {
        throw Error('Deadline or post-timeout recovery failed');
      }
      return true;
    }
    throw Error('Unanswered request unexpectedly resolved');
  }
});
