/**
 * Native file drag and drop main-process reference. Manual native drag interaction. The drag
 * file is fixed in main; renderer input is not a filesystem path. Firefly
 * nativeImage/startDrag/IPC are absent.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'native-file-drag-drop',
  requires: ['nativeImage.createFromPath'],
  earlyRequires: [],
  smoke: false,
  actions: {
    drag: async ({ api, window, directory }) => {
      const { join } = await import('node:path');
      window.webContents.startDrag({
        file: join(directory, 'note.txt'),
        icon: api.nativeImage.createFromPath(join(directory, 'icon.png'))
      });
    }
  }
});
