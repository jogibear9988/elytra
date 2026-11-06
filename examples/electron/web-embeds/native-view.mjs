import { runExample } from '../shared/run.mjs';
await runExample(import.meta.url, {
  id: 'web-embeds-native-view',
  requires: ['WebContentsView'],
  smoke: false,
  setup: async ({ api, window, directory }) => {
    const { join } = await import('node:path');
    const view = new api.WebContentsView({
      webPreferences: { sandbox: true, nodeIntegration: false, contextIsolation: true }
    });
    window.contentView.addChildView(view);
    view.setBounds({ x: 0, y: 80, width: 800, height: 450 });
    await view.webContents.loadFile(join(directory, 'embedded.html'));
    window.on('closed', () => {
      if (!view.webContents.isDestroyed()) view.webContents.close();
    });
  }
});
