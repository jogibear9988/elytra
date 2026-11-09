/** Shared hidden-window smoke check; native provider enforcement has a separate probe. */
import { app, BrowserWindow, WebContentsView } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(async () => {
  let window;
  let child;
  try {
    const preferences = Object.freeze({ spellcheck: false });
    window = new BrowserWindow({ show: false, webPreferences: preferences });
    await window.loadFile(path.join(import.meta.dirname, 'editor.html'));
    assert.equal(window.isVisible(), false);
    child = new WebContentsView({ webPreferences: preferences });
    window.contentView.addChildView(child);
    await child.webContents.loadFile(path.join(import.meta.dirname, 'editor.html'));
    assert.equal(preferences.spellcheck, false);
    console.log('PASS public spellcheck false window and child load real documents while hidden');
    if (process.versions.mite) {
      const count = BrowserWindow.getAllWindows().length;
      for (const Constructor of [BrowserWindow, WebContentsView]) {
        assert.throws(
          () => new Constructor({ webPreferences: { spellcheck: true } }),
          (error) => error.code === 'ERR_FIREFLY_UNSUPPORTED_API'
        );

        for (const value of [undefined, null, 0, 'false', {}]) {
          assert.throws(
            () => new Constructor({ webPreferences: { spellcheck: value } }),
            (error) => error.code === 'ERR_INVALID_ARG_TYPE'
          );
        }
      }
      assert.equal(BrowserWindow.getAllWindows().length, count);
      console.log(
        'PASS Firefly spellcheck enabled and nonboolean requests reject before allocation'
      );
    }
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
