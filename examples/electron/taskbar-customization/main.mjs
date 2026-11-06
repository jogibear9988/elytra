import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'taskbar-customization',
  requires: [
    'BrowserWindow.prototype.setOverlayIcon',
    'BrowserWindow.prototype.setThumbarButtons',
    'nativeImage.createFromPath'
  ],
  earlyRequires: [],
  smoke: false,
  platforms: ['win32'],
  setup: async ({ api, window, directory, smoke, pageURL }) => {
    const { join } = await import('node:path');
    const icon = api.nativeImage.createFromPath(join(directory, 'icon.png'));
    window.setOverlayIcon(icon, 'Elytra example');
    if (
      !window.setThumbarButtons([
        { tooltip: 'Report title', icon, click: () => console.log(window.webContents.getTitle()) }
      ])
    )
      throw Error('Thumbnail toolbar unavailable');
  }
});
