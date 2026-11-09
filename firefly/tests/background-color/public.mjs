/** Shared public native color API; pixel acceptance belongs to the separate native probe. */
import { app, BrowserWindow, WebContentsView } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const deadline = setTimeout(() => app.exit(1), 15000);

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
    assert.equal(window.getBackgroundColor(), '#123456');
    await window.loadFile(path.join(import.meta.dirname, 'blank.html'));
    assert.equal(window.getBackgroundColor(), '#123456');
    console.log('PASS public background constructor native getter survives actual renderer load');
    for (const [input, expected] of [
      ['#abc', '#AABBCC'],
      ['#2468ac', '#2468AC'],
      ['rgb(255, 145, 17)', '#FF9111'],
      ['hsl(120, 100%, 25%)', '#008000'],
      ['red', '#FF0000'],
      ['transparent', '#FFFFFF'],
      ['not-a-color', '#FFFFFF']
    ]) {
      assert.equal(window.setBackgroundColor(input), undefined);
      assert.equal(window.getBackgroundColor(), expected);
    }
    for (const value of [null, undefined, 123, {}, true]) {
      assert.throws(() => window.setBackgroundColor(value), TypeError);

      assert.equal(window.getBackgroundColor(), '#FFFFFF');
    }
    console.log(
      'PASS public background opaque CSS colors invalid fallback and strict setter types'
    );
    window.setBackgroundColor('#2468ac');
    await window.loadFile(path.join(import.meta.dirname, 'blank.html'));
    assert.equal(window.getBackgroundColor(), '#2468AC');
    assert.equal(window.isVisible(), false);
    console.log('PASS public background setter persists document replacement with window hidden');
    child = new WebContentsView();
    assert.equal(typeof child.getBackgroundColor, 'undefined');
    assert.equal(typeof child.webContents.setBackgroundColor, 'undefined');
    assert.equal(typeof child.webContents.getBackgroundColor, 'undefined');
    assert.equal(child.setBackgroundColor('#0a2540'), undefined);

    assert.throws(() => child.setBackgroundColor(null), TypeError);

    child.setBounds({ x: 0, y: 0, width: 96, height: 64 });
    window.contentView.addChildView(child);
    await child.webContents.loadFile(path.join(import.meta.dirname, 'blank.html'));
    assert.equal(window.getBackgroundColor(), '#2468AC');
    console.log(
      'PASS public background independent child view setter and genuine public API shape'
    );
    if (process.versions.mite) {
      for (const value of ['#8123', '#80123456', 'rgba(30, 60, 90, 0.5)']) {
        assert.throws(
          () => window.setBackgroundColor(value),
          (error) => error.code === 'ERR_FIREFLY_UNSUPPORTED_API'
        );

        assert.equal(window.getBackgroundColor(), '#2468AC');

        assert.throws(
          () => child.setBackgroundColor(value),
          (error) => error.code === 'ERR_FIREFLY_UNSUPPORTED_API'
        );
      }
      console.log(
        'PASS Firefly background nonopaque colors explicitly unsupported without state mutation'
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
