/**
 * Navigation history main-process reference. Uses the current navigationHistory object, not
 * deprecated WebContents goBack aliases. Firefly host history APIs and IPC remain absent.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'navigation-history',
  requires: [],
  instanceRequires: [
    'webContents.navigationHistory.canGoBack',
    'webContents.navigationHistory.goBack',
    'webContents.navigationHistory.goForward'
  ],
  earlyRequires: [],
  smoke: false,
  actions: {
    back: ({ window }) => {
      const history = window.webContents.navigationHistory;
      if (history.canGoBack()) {
        history.goBack();
      }
    },

    forward: ({ window }) => {
      const history = window.webContents.navigationHistory;
      if (history.canGoForward()) {
        history.goForward();
      }
    }
  }
});
