/** Real hidden BrowserWindows; native traffic-light and style proof is a separate Cocoa probe. */
import { app, BrowserWindow } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const mite = !!process.versions.mite;
const cocoa = process.env.QT_QPA_PLATFORM === 'cocoa';
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(() => {
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
      console.log(
        'PASS Firefly title bar hidden explicitly unsupported offscreen before native admission'
      );
    }
    if (mite) {
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
