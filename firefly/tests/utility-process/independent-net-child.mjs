/** Real utility Electron-net transport: no defaultSession was selected at fork. */
import { net } from 'firefly';

process.parentPort.on('message', async ({ data }) => {
  try {
    const response = await net.fetch(data);
    process.parentPort.postMessage({ status: response.status, body: await response.text() });
  } catch (error) {
    process.parentPort.postMessage({ error: String(error) });
  }
});

process.parentPort.postMessage({ ready: true });
