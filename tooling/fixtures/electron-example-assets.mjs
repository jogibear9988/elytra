import { serveAssets } from '../../examples/electron/shared/assets.mjs';
const { server, url } = await serveAssets(
  new URL('../../examples/electron/quick-start/main.mjs', import.meta.url),
  'quick-start'
);
console.log(JSON.stringify({ url }));
// Bound lifetime even if the controlling test process disappears.
setTimeout(() => server.close(), 30000);
