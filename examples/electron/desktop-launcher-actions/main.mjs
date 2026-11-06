/**
 * Desktop launcher actions main-process reference. Linux manual packaging example. The
 * .desktop template must be installed with a real absolute executable path; Firefly Linux
 * desktop packaging remains unavailable.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'desktop-launcher-actions',
  requires: [],
  earlyRequires: [],
  smoke: false,
  platforms: ['linux'],

  setup: async ({ api, window, directory, smoke, pageURL }) => {
    console.log(
      'Desktop action:',
      process.argv.includes('--new-window') ? 'new-window' : 'default'
    );
  }
});
