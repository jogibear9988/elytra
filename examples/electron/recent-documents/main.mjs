/**
 * Recent documents main-process reference. This changes the user’s recent-document list only
 * after an explicit button click. Tests do not clear or modify that list. Firefly native
 * recent-document APIs are absent.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'recent-documents',
  requires: ['app.addRecentDocument', 'app.clearRecentDocuments'],
  earlyRequires: [],
  smoke: false,
  platforms: ['darwin', 'win32'],
  actions: {
    add: async ({ api, directory }) => {
      const { join } = await import('node:path');
      api.app.addRecentDocument(join(directory, 'note.txt'));
    }
  }
});
