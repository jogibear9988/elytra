/**
 * Multithreading main-process reference. Browser-only module workers; Node/Mite APIs in
 * workers and nodeIntegrationInWorker remain unsupported. File-worker loading and worker
 * execution require their own native test result.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'multithreading',
  requires: [],
  earlyRequires: [],
  smoke: true
});
