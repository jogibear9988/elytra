/**
 * Dark mode main-process reference. Firefly lacks nativeTheme and public IPC. CSS-only
 * theming does not establish native theme compatibility.
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
