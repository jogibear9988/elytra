/**
 * Online and offline events main-process reference. Smoke validates initial browser
 * state/listener installation only. Real OS network transition delivery is a separate manual
 * gate; onLine is not proof of internet reachability.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'online-offline',
  requires: [],
  earlyRequires: [],
  smoke: true
});
