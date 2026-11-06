/**
 * Deep links main-process reference. No protocol registration occurs without the explicit
 * environment opt-in. Packaging needs OS protocol declarations. OS delivery and single-
 * instance handling are unsupported in Firefly.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'deep-links',
  requires: [],
  earlyRequires: ['app.setAsDefaultProtocolClient', 'app.requestSingleInstanceLock'],
  smoke: false,

  beforeReady: async (api) => {
    if (!api.app.requestSingleInstanceLock()) {
      api.app.quit();
      return false;
    }

    api.app.on('open-url', (event, url) => {
      event.preventDefault();
      if (url.startsWith('elytra-example://')) {
        console.log(url);
      }
    });

    api.app.on('second-instance', (_event, argv) => {
      for (const value of argv) {
        if (value.startsWith('elytra-example://')) {
          console.log(value);
        }
      }
    });

    if (process.env.ELYTRA_REGISTER_PROTOCOL === '1') {
      api.app.setAsDefaultProtocolClient('elytra-example');
    }
  }
});
