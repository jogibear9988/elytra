/** Native probe frontend: no capture API or test-only JavaScript host capability. */
import { app, BrowserWindow, WebContentsView } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const deadline = setTimeout(() => {
  console.error('Native background probe timed out');
  app.exit(1);
}, 22000);

app.whenReady().then(async () => {
  let window;
  let child;
  try {
    window = new BrowserWindow({
      show: false,
      width: 320,
      height: 220,
      backgroundColor: '#123456'
    });

    async function acknowledge(title, expected, label) {
      const end = Date.now() + 3000;
      window.setTitle(title);
      while (window.getTitle() !== expected) {
        assert.ok(Date.now() < end, `Native compositor/presentation did not verify ${label}`);
        if (window.getTitle() !== title) {
          window.setTitle(title);
        }
        await new Promise((resolve) => setTimeout(resolve, 20));
      }
    }

    const stage = (index) =>
      acknowledge(
        `elytra-background-stage-${index}`,
        `elytra-background-ok-${index}`,
        `background stage ${index}`
      );

    const load = (name) => window.loadFile(path.join(import.meta.dirname, name));

    await stage(0);
    // The static offscreen plugin has no desktop windows. Virtual visibility is
    // required for real renderer frame production; keep physical backends forbidden.
    assert.equal(process.env.QT_QPA_PLATFORM, 'offscreen');
    window.show();
    await load('blank.html');
    await stage(1);
    window.setBackgroundColor('#2468ac');
    await stage(2);
    await load('authored.html');
    await stage(3);
    window.setBackgroundColor('#4c6e2a');
    await stage(4);
    await load('blank.html');
    await stage(5);
    child = new WebContentsView();
    child.setBounds({ x: 0, y: 0, width: 96, height: 64 });
    child.setBackgroundColor('#0a2540');
    window.contentView.addChildView(child);
    await acknowledge(
      'elytra-background-arm-child',
      'elytra-background-armed-child',
      'child compositor observer before navigation'
    );
    await child.webContents.loadFile(path.join(import.meta.dirname, 'blank.html'));
    await stage(6);
    assert.equal(window.isVisible(), true);
    console.log(
      'PASS native background blank author override live update navigation and independent child view'
    );
    child.webContents.close({ waitForBeforeUnload: false });
    window.destroy();
    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    console.error(error);
    child?.webContents?.close({ waitForBeforeUnload: false });
    window?.destroy();
    clearTimeout(deadline);
    app.exit(1);
  }
});
