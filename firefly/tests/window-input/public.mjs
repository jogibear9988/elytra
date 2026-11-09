/** Hidden actual BrowserWindow admission; native first-click behavior has its own Cocoa probe. */
import { app, BrowserWindow, WebContentsView } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const platform = process.env.QT_QPA_PLATFORM;
const mite = !!process.versions.mite;
assert.ok(!mite || platform === 'cocoa' || platform === 'offscreen');
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(() => {
  const windows = [];
  let child;
  try {
    function create(options = {}) {
      const window = new BrowserWindow({ show: false, width: 320, height: 220, ...options });
      windows.push(window);
      assert.equal(window.isVisible(), false);
      return window;
    }

    create();
    console.log('PASS public window input default hidden native window admission');
    for (const value of [true, false]) {
      create({ experimentalDarkMode: value });
    }
    console.log(
      'PASS public window input legacy experimentalDarkMode booleans admitted without appearance claim'
    );
    const firstMouseHosts = [];
    for (const value of [true, false]) {
      if (!mite || platform === 'cocoa') {
        firstMouseHosts.push(create({ acceptFirstMouse: value }));
      } else {
        const count = BrowserWindow.getAllWindows().length;

        assert.throws(
          () => create({ acceptFirstMouse: value }),
          (failure) => failure.code === 'ERR_FIREFLY_UNSUPPORTED_API'
        );

        assert.equal(BrowserWindow.getAllWindows().length, count);
      }
    }
    console.log(
      `PASS public window input explicit first mouse ${!mite || platform === 'cocoa' ? 'native admission' : 'offscreen unsupported before admission'}`
    );
    const falseHost = firstMouseHosts[1] ?? windows[0];
    const trueHost = firstMouseHosts[0] ?? windows[1];
    child = new WebContentsView();
    const contents = child.webContents;
    child.setBounds({ x: 0, y: 0, width: 96, height: 64 });
    falseHost.contentView.addChildView(child);
    assert.equal(BrowserWindow.fromWebContents(contents), falseHost);
    trueHost.contentView.addChildView(child);
    assert.equal(child.webContents, contents);
    assert.equal(BrowserWindow.fromWebContents(contents), trueHost);
    trueHost.contentView.removeChildView(child);
    // Firefly explicitly clears detached ownership; Electron retains its last
    // host here. This existing documented difference is unrelated to click policy.
    assert.equal(BrowserWindow.fromWebContents(contents), mite ? null : trueHost);
    assert.ok(windows.every((window) => !window.isVisible()));
    console.log(
      'PASS public window input child view attach move detach preserves owned native identity'
    );
    if (mite) {
      for (const name of ['acceptFirstMouse', 'experimentalDarkMode']) {
        for (const value of [null, 0, 1, 'true', {}, []]) {
          const count = BrowserWindow.getAllWindows().length;

          assert.throws(
            () => create({ [name]: value }),
            (failure) => failure.code === 'ERR_INVALID_ARG_TYPE'
          );

          assert.equal(BrowserWindow.getAllWindows().length, count);
        }
      }
      const count = BrowserWindow.getAllWindows().length;

      assert.throws(
        () => create({ notAnElectronWindowOption: true }),
        (failure) => failure.code === 'ERR_FIREFLY_UNSUPPORTED_API'
      );

      assert.equal(BrowserWindow.getAllWindows().length, count);
      console.log(
        'PASS public window input invalid types and unknown option fail before native admission'
      );
    }

    child.webContents.close({ waitForBeforeUnload: false });

    windows.forEach((window) => window.destroy());

    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    console.error(error);
    child?.webContents?.close({ waitForBeforeUnload: false });

    windows.forEach((window) => window.destroy());

    clearTimeout(deadline);
    app.exit(1);
  }
});
