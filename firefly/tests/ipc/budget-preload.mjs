/** Saturate native host budgets from separate renderer processes. */
import { contextBridge, ipcRenderer } from 'firefly';

contextBridge.exposeInMainWorld('desktop', {
  async saturate() {
    const calls = Array.from({ length: 16 }, () => ipcRenderer.invoke('held'));
    const results = await Promise.allSettled(calls);
    if (results.every((result) => result.status === 'fulfilled')) {
      return true;
    }
    if (
      !results.every(
        (result) =>
          result.status === 'rejected' && result.reason.message.includes('budget exceeded')
      )
    ) {
      throw Error('Inconsistent global budget rejection');
    }
    return false;
  },

  async recover() {
    return (await ipcRenderer.invoke('held')) === 'RECOVERED';
  }
});
