/** Native utility-process public API over real owned loopback peers. */
import { net } from 'firefly';
import exerciseNet from './transport.mjs';
try {
  const config = JSON.parse(process.env.ELYTRA_NET_TEST);
  await exerciseNet(net, { ...config, label: 'utility' });
  process.parentPort.postMessage({ ok: true });
  process.parentPort.close?.();
  process.exit(0);
} catch (error) {
  process.parentPort.postMessage({ error: String(error.stack || error) });
  process.exit(1);
}
