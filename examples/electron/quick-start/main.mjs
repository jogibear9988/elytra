import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'quick-start',
  requires: [],
  earlyRequires: [],
  smoke: true,
  preload: true
});
