/**
 * Quick start main-process reference. Real isolated preload and renderer startup;
 * activation/dock behavior is not implemented.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'quick-start',
  requires: [],
  earlyRequires: [],
  smoke: true,
  preload: true
});
