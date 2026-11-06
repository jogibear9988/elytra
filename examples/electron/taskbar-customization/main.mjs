/**
 * Taskbar customization main-process reference. Windows-only native taskbar UI. Firefly
 * Windows desktop and these methods are not implemented; macOS/Linux checks are explicit
 * platform errors.
 */
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
    ) {
      throw Error('Thumbnail toolbar unavailable');
    }
  }
});
