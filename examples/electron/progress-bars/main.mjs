/**
 * Progress bars main-process reference. Native taskbar/dock progress, not an HTML progress-
 * element substitute. Firefly setProgressBar is absent.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'progress-bars',
  requires: ['BrowserWindow.prototype.setProgressBar'],
  earlyRequires: [],
  smoke: false,

  setup: async ({ api, window, directory, smoke, pageURL }) => {
    let progress = 0;
    const timer = setInterval(() => {
      progress += 0.1;
      if (progress >= 1) {
        window.setProgressBar(-1);
        clearInterval(timer);
      } else {
        window.setProgressBar(progress);
      }
    }, 500);

    window.on('closed', () => clearInterval(timer));
  }
});
