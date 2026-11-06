import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'represented-file',
  requires: [
    'BrowserWindow.prototype.setRepresentedFilename',
    'BrowserWindow.prototype.setDocumentEdited'
  ],
  earlyRequires: [],
  smoke: false,
  platforms: ['darwin'],
  setup: async ({ api, window, directory, smoke, pageURL }) => {
    const { join } = await import('node:path');
    window.setRepresentedFilename(join(directory, 'note.txt'));
    window.setDocumentEdited(true);
  }
});
