/** Actual production Qt constraint gate, with every window kept hidden. */
import { app, BrowserWindow } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(async () => {
  let window;
  try {
    assert.equal(process.env.QT_QPA_PLATFORM, 'offscreen');
    window = new BrowserWindow({
      show: false,
      width: 320,
      height: 220,
      minWidth: 240,
      minHeight: 160,
      maxWidth: 500,
      maxHeight: 400
    });

    async function stage(index) {
      const expected = `elytra-constraints-ok-${index}`;
      const end = Date.now() + 3000;
      window.setTitle(`elytra-constraints-stage-${index}`);
      while (window.getTitle() !== expected) {
        assert.notEqual(
          window.getTitle(),
          'elytra-constraints-failed',
          `Native Qt constraint failure at stage ${index}`
        );
        assert.ok(Date.now() < end, `Native Qt constraint timeout at stage ${index}`);
        await new Promise((resolve) => setTimeout(resolve, 20));
      }
    }

    await stage(0);
    window.setMinimumSize(280, 180);
    window.setMaximumSize(460, 360);
    await stage(1);
    window.setMinimumSize(0, 0);
    window.setMaximumSize(0, 0);
    await stage(2);
    assert.equal(window.isVisible(), false);
    window.destroy();
    clearTimeout(deadline);
    console.log(
      'PASS native constraints constructor setters reset and actual hidden Qt enforcement'
    );
    app.exit(0);
  } catch (error) {
    console.error(error);
    window?.destroy();
    clearTimeout(deadline);
    app.exit(1);
  }
});
