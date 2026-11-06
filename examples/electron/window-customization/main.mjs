import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'window-customization',
  requires: [],
  earlyRequires: [],
  smoke: true,
  options: { frame: false }
});
