/** Real hidden BrowserWindows; native traffic-light and style proof is a separate Cocoa probe. */
import { app, BrowserWindow } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const mite = !!process.versions.mite;
const cocoa = process.env.QT_QPA_PLATFORM === 'cocoa';
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(async () => {
  const windows = [];
  try {
    function create(options = {}) {
      const window = new BrowserWindow({ show: false, width: 320, height: 220, ...options });
      windows.push(window);
      assert.equal(window.isVisible(), false);
      return window;
    }

    const size = (bounds) => [bounds.width, bounds.height];

    const standard = create();
    const explicit = create({ titleBarStyle: 'default' });
    assert.deepEqual(size(standard.getBounds()), [320, 220]);
    assert.deepEqual(size(explicit.getBounds()), [320, 220]);
    assert.deepEqual(size(standard.getContentBounds()), size(explicit.getContentBounds()));
    console.log(
      'PASS public title bar omitted and explicit default preserve native outer content geometry'
    );
    if (!mite || cocoa) {
      const hidden = create({ titleBarStyle: 'hidden' });
      if (process.platform === 'darwin') {
        assert.equal(hidden.getWindowButtonPosition(), null);
        hidden.setWindowButtonPosition({ x: 18, y: 7 });
        assert.deepEqual(hidden.getWindowButtonPosition(), { x: 18, y: 7 });
        hidden.setWindowButtonPosition(null);
        assert.equal(hidden.getWindowButtonPosition(), null);
        console.log('PASS public title bar window button position and reset');
      }
      const outer = hidden.getBounds();
      const content = hidden.getContentBounds();
      assert.deepEqual(size(outer), [320, 220]);
      assert.deepEqual(size(content), size(outer));
      assert.ok(standard.getContentBounds().height <= standard.getBounds().height);
      hidden.setMinimumSize(240, 160);
      hidden.setMaximumSize(500, 400);
      hidden.setBounds({ width: 100, height: 80 });
      assert.deepEqual(size(hidden.getBounds()), [240, 160]);
      assert.deepEqual(size(hidden.getContentBounds()), [240, 160]);
      hidden.setBounds({ width: 900, height: 700 });
      assert.deepEqual(size(hidden.getBounds()), [500, 400]);
      assert.deepEqual(size(hidden.getContentBounds()), [500, 400]);
      const byContent = create({ titleBarStyle: 'hidden', useContentSize: true });
      assert.deepEqual(size(byContent.getContentBounds()), [320, 220]);
      assert.deepEqual(size(byContent.getBounds()), [320, 220]);
      const combined = create({ titleBarStyle: 'hidden', frame: false });
      assert.deepEqual(size(combined.getContentBounds()), [320, 220]);
      assert.deepEqual(size(combined.getBounds()), [320, 220]);
      console.log(
        'PASS public title bar hidden full content geometry and real constrained resizing'
      );
    } else {
      const count = BrowserWindow.getAllWindows().length;

      assert.throws(
        () => create({ titleBarStyle: 'hidden' }),
        (failure) => failure.code === 'ERR_FIREFLY_UNSUPPORTED_API'
      );

      assert.equal(BrowserWindow.getAllWindows().length, count);

      assert.throws(() => standard.getWindowButtonPosition(), {
        code: 'ERR_FIREFLY_UNSUPPORTED_API'
      });

      assert.throws(() => standard.setWindowButtonPosition({ x: 18, y: 7 }), {
        code: 'ERR_FIREFLY_UNSUPPORTED_API'
      });

      console.log(
        'PASS Firefly title bar hidden explicitly unsupported offscreen before native admission'
      );
    }
    if (mite && cocoa) {
      const overlay = create({ titleBarStyle: 'hidden', titleBarOverlay: true });

      async function observed(title) {
        for (let retry = 0; retry < 100; ++retry) {
          const observedTitle = overlay.webContents.getTitle();
          if (observedTitle === title) {
            return;
          }
          assert.ok(!observedTitle.startsWith('FAIL'), observedTitle);
          await new Promise((resolve) => setTimeout(resolve, 20));
        }
        throw Error(
          'Missing overlay lifecycle step: ' + title + ' (' + overlay.webContents.getTitle() + ')'
        );
      }

      await overlay.loadFile(path.join(import.meta.dirname, 'overlay.html'));
      await observed('Overlay ready');
      overlay.setWindowButtonPosition({ x: 36, y: 7 });
      await observed('Overlay moved');
      overlay.setBounds({ width: 400, height: 220 });
      await observed('Overlay resized');
      console.log(
        'PASS public Cocoa overlay initial geometry, resize events, and iframe exclusion'
      );
    }
    if (mite) {
      assert.throws(() => create({ titleBarOverlay: true }), {
        code: 'ERR_FIREFLY_UNSUPPORTED_API'
      });

      for (const value of [null, true, 0, {}, []]) {
        const count = BrowserWindow.getAllWindows().length;

        assert.throws(
          () => create({ titleBarStyle: value }),
          (failure) => failure.code === 'ERR_INVALID_ARG_TYPE'
        );

        assert.equal(BrowserWindow.getAllWindows().length, count);
      }
      for (const value of ['hiddenInset', 'customButtonsOnHover', 'unknown', '']) {
        const count = BrowserWindow.getAllWindows().length;

        assert.throws(
          () => create({ titleBarStyle: value }),
          (failure) => failure.code === 'ERR_FIREFLY_UNSUPPORTED_API'
        );

        assert.equal(BrowserWindow.getAllWindows().length, count);
      }
      console.log(
        'PASS Firefly title bar strict option types and unknown styles reject before native admission'
      );
    }
    assert.ok(windows.every((window) => !window.isVisible()));

    windows.forEach((window) => window.destroy());

    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    console.error(error);

    windows.forEach((window) => window.destroy());

    clearTimeout(deadline);
    app.exit(1);
  }
});
