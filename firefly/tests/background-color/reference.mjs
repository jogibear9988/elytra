/** One hidden Electron oracle; captures actual blank renderer pixels, never desktop pixels. */
import { app, BrowserWindow, WebContentsView } from 'electron';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const timeout = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(async () => {
  let window;
  let child;
  try {
    window = new BrowserWindow({
      show: false,
      width: 300,
      height: 200,
      backgroundColor: '#123456'
    });
    const results = {
      constructor: window.getBackgroundColor(),
      webContentsMethods: [
        typeof window.webContents.getBackgroundColor,
        typeof window.webContents.setBackgroundColor
      ]
    };
    await window.loadFile(path.join(import.meta.dirname, 'blank.html'));

    async function pixel() {
      const image = await window.webContents.capturePage();
      const bytes = image.toBitmap();
      const size = image.getSize();
      const index = (Math.floor(size.height / 2) * size.width + Math.floor(size.width / 2)) * 4;
      return { size, rgba: [bytes[index + 2], bytes[index + 1], bytes[index], bytes[index + 3]] };
    }

    results.constructorPixel = await pixel();
    results.colors = [];
    for (const color of [
      '#abc',
      '#8123',
      '#80123456',
      'rgb(255, 145, 17)',
      'rgba(30, 60, 90, 0.5)',
      'hsl(120, 100%, 25%)',
      'red',
      'transparent',
      'not-a-color'
    ]) {
      window.setBackgroundColor(color);
      results.colors.push({ input: color, get: window.getBackgroundColor(), pixel: await pixel() });
    }
    results.badTypes = [];
    for (const color of [null, undefined, 123, {}, true]) {
      try {
        window.setBackgroundColor(color);
        results.badTypes.push({ input: String(color), accepted: window.getBackgroundColor() });
      } catch (error) {
        results.badTypes.push({ input: String(color), name: error.name, message: error.message });
      }
    }
    window.setBackgroundColor('#2468ac');
    await window.loadFile(path.join(import.meta.dirname, 'blank.html'));
    results.navigation = { get: window.getBackgroundColor(), pixel: await pixel() };
    child = new WebContentsView();
    results.view = {
      getter: typeof child.getBackgroundColor,
      setter: typeof child.setBackgroundColor
    };
    console.log(JSON.stringify(results));
    clearTimeout(timeout);
    child.webContents.close({ waitForBeforeUnload: false });
    window.destroy();
    app.exit(0);
  } catch (error) {
    console.error(error);
    child?.webContents?.close({ waitForBeforeUnload: false });
    window?.destroy();
    app.exit(1);
  }
});
