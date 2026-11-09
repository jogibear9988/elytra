/** One explicitly coordinated visible Cocoa batch; no synthetic mouse or native-control activation. */
import { app, BrowserWindow } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'runtime-config'));
let window;
const deadline = setTimeout(() => {
  console.error('Title bar runtime acceptance timed out');
  window?.hide();
  app.exit(1);
}, 12000);

app.whenReady().then(async () => {
  try {
    window = new BrowserWindow({
      title: 'elytra-title-bar-runtime',
      titleBarStyle: 'hidden',
      width: 640,
      height: 400,
      show: false,
      backgroundColor: '#E0E6EE'
    });
    await window.loadFile(path.join(import.meta.dirname, 'page.html'));
    window.setTitle('elytra-title-bar-runtime');
    window.show();
    assert.equal(window.isVisible(), true);
    while (window.getTitle() !== 'elytra-title-bar-runtime-ok') {
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    window.hide();
    assert.equal(window.isVisible(), false);
    console.log('PASS public title bar actual renderer toolbar and native controls runtime');
    clearTimeout(deadline);
    window.destroy();
    app.exit(0);
  } catch (error) {
    console.error(error);
    clearTimeout(deadline);
    window?.hide();
    window?.destroy();
    app.exit(1);
  }
});
