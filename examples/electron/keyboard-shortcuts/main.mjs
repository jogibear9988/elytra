import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'keyboard-shortcuts',
  requires: ['Menu.buildFromTemplate', 'globalShortcut.register'],
  earlyRequires: [],
  smoke: false,
  setup: async ({ api, window, directory, smoke, pageURL }) => {
    const items = [
      {
        label: 'Example',
        submenu: [
          {
            label: 'Report shortcut',
            accelerator: 'CmdOrCtrl+Shift+Y',
            click: () => console.log('LOCAL_SHORTCUT')
          },
          { role: 'quit' }
        ]
      }
    ];
    api.Menu.setApplicationMenu(api.Menu.buildFromTemplate(items));
    if (
      !api.globalShortcut.register('CommandOrControl+Shift+Y', () => console.log('GLOBAL_SHORTCUT'))
    )
      throw Error('Shortcut unavailable');
    api.app.on('will-quit', () => api.globalShortcut.unregisterAll());
  }
});
