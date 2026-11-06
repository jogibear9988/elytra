/**
 * Menus main-process reference. Application and context menus use the real Menu API. Firefly
 * Menu/MenuItem are not implemented.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'menus',
  requires: ['Menu.buildFromTemplate'],
  earlyRequires: [],
  smoke: false,

  setup: async ({ api, window, directory, smoke, pageURL }) => {
    const template = [
      {
        label: 'Example',
        submenu: [
          { label: 'Inspect window', click: () => console.log(window.webContents.getTitle()) },
          { type: 'separator' },
          { role: 'quit' }
        ]
      }
    ];
    api.Menu.setApplicationMenu(api.Menu.buildFromTemplate(template));

    window.webContents.on('context-menu', () =>
      api.Menu.buildFromTemplate([
        { label: 'Context action', click: () => console.log('CONTEXT_MENU') }
      ]).popup({ window })
    );
  }
});
