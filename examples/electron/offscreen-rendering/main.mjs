/**
 * Offscreen rendering main-process reference. Real offscreen paint events, not screenshots
 * relabeled as offscreen rendering. Firefly offscreen/paint API is absent; no frame-rate
 * performance claim.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'offscreen-rendering',
  requires: [],
  earlyRequires: ['app.disableHardwareAcceleration'],
  smoke: false,
  options: { show: false, webPreferences: { offscreen: true } },

  beforeReady: async (api) => {
    api.app.disableHardwareAcceleration();
  },

  setup: async ({ api, window, directory, smoke, pageURL }) => {
    window.webContents.setFrameRate(30);
    let frames = 0;

    window.webContents.on('paint', (_event, dirty, image) => {
      if (++frames === 1) {
        console.log(JSON.stringify({ dirty, pngBytes: image.toPNG().length }));
      }
    });
  }
});
