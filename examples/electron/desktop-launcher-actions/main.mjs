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
