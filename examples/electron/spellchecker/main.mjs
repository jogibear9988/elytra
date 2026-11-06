/**
 * Spellchecker main-process reference. Native spell suggestions/context menu require manual
 * validation and dictionaries. Firefly spellcheck constructor option/session APIs are
 * unsupported.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'spellchecker',
  requires: ['Menu.buildFromTemplate'],
  earlyRequires: [],
  smoke: false,
  options: { webPreferences: { spellcheck: true } },

  setup: async ({ api, window, directory, smoke, pageURL }) => {
    if (process.platform !== 'darwin') {
      window.webContents.session.setSpellCheckerLanguages(['en-US']);
    }

    window.webContents.on('context-menu', (_event, params) => {
      const entries = params.dictionarySuggestions.map((word) => ({
        label: word,

        click: () => window.webContents.replaceMisspelling(word)
      }));
      if (params.misspelledWord) {
        entries.push({
          label: 'Add word',

          click: () =>
            window.webContents.session.addWordToSpellCheckerDictionary(params.misspelledWord)
        });
      }
      if (entries.length) {
        api.Menu.buildFromTemplate(entries).popup({ window });
      }
    });
  }
});
