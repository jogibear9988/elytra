/**
 * Dark mode main-process example using native theme overrides and isolated IPC.
 * Direct API/CSS fixtures pass; this example still requires manual button/visual acceptance.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'dark-mode',
  requires: ['nativeTheme.themeSource'],
  earlyRequires: [],
  smoke: false,
  actions: {
    toggle: ({ api }) => {
      api.nativeTheme.themeSource = api.nativeTheme.shouldUseDarkColors ? 'light' : 'dark';
      return api.nativeTheme.shouldUseDarkColors;
    },

    system: ({ api }) => {
      api.nativeTheme.themeSource = 'system';
      return api.nativeTheme.shouldUseDarkColors;
    }
  }
});
