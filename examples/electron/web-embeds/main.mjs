/**
 * Web embeds main-process reference. Runnable browser iframe slice. Native
 * WebContentsView/webview and session/navigation controls remain unsupported; no Chrome
 * extension embedding claim.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'web-embeds',
  requires: [],
  earlyRequires: [],
  smoke: true
});
