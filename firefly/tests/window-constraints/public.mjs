/** Hidden public geometry checks exercise native resize enforcement, not only getters. */
import { app, BrowserWindow } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(() => {
  const windows = [];
  try {
    function create(options = {}) {
      const window = new BrowserWindow({ show: false, width: 320, height: 220, ...options });
      windows.push(window);
      return window;
    }

    const size = (window) => {
      const bounds = window.getBounds();
      return [bounds.width, bounds.height];
    };

    const standard = create();
    assert.deepEqual(standard.getMinimumSize(), [0, 0]);
    assert.deepEqual(standard.getMaximumSize(), [0, 0]);
    standard.getMinimumSize()[0] = 999;
    assert.deepEqual(standard.getMinimumSize(), [0, 0]);
    console.log('PASS public constraints defaults and fresh native limit snapshots');
    const minimum = create({ width: 100, height: 80, minWidth: 240, minHeight: 160 });
    assert.deepEqual(minimum.getMinimumSize(), [240, 160]);
    assert.deepEqual(size(minimum), [240, 160]);
    const maximum = create({ width: 700, height: 600, maxWidth: 500, maxHeight: 400 });
    assert.deepEqual(maximum.getMaximumSize(), [500, 400]);
    assert.deepEqual(size(maximum), [500, 400]);
    console.log('PASS public constraints constructor limits clamp real initial outer bounds');
    assert.equal(standard.setMinimumSize(240, 160), undefined);
    assert.equal(standard.setMaximumSize(500, 400), undefined);
    assert.deepEqual(standard.getMinimumSize(), [240, 160]);
    assert.deepEqual(standard.getMaximumSize(), [500, 400]);
    standard.setBounds({ width: 100, height: 80 });
    assert.deepEqual(size(standard), [240, 160]);
    standard.setBounds({ width: 900, height: 700 });
    assert.deepEqual(size(standard), [500, 400]);
    standard.setBounds({ width: 320, height: 220 });
    assert.deepEqual(size(standard), [320, 220]);
    console.log('PASS public constraints setters enforce both real outer resize limits');
    const content = create({
      useContentSize: true,
      minWidth: 240,
      minHeight: 160,
      maxWidth: 500,
      maxHeight: 400
    });
    const outerBounds = content.getBounds();
    const contentBounds = content.getContentBounds();
    const frame = [
      outerBounds.width - contentBounds.width,
      outerBounds.height - contentBounds.height
    ];
    assert.deepEqual(content.getMinimumSize(), [240 + frame[0], 160 + frame[1]]);
    assert.deepEqual(content.getMaximumSize(), [500 + frame[0], 400 + frame[1]]);
    content.setMinimumSize(260, 180);
    content.setMaximumSize(520, 420);
    assert.deepEqual(content.getMinimumSize(), [260, 180]);
    assert.deepEqual(content.getMaximumSize(), [520, 420]);
    content.setBounds({ width: 100, height: 80 });
    assert.deepEqual(size(content), [260, 180]);
    console.log(
      'PASS public constraints content constructor translates frames while setters stay outer'
    );
    standard.setMinimumSize(0, 0);
    standard.setMaximumSize(0, 350);
    assert.deepEqual(standard.getMaximumSize(), [0, 350]);
    standard.setBounds({ width: 700, height: 600 });
    assert.deepEqual(size(standard), [700, 350]);
    standard.setMaximumSize(450, 0);
    assert.deepEqual(standard.getMaximumSize(), [450, 0]);
    standard.setBounds({ width: 700, height: 600 });
    assert.deepEqual(size(standard), [450, 600]);
    standard.setMaximumSize(0, 0);
    assert.deepEqual(standard.getMaximumSize(), [0, 0]);
    standard.setBounds({ width: 701, height: 503 });
    assert.deepEqual(size(standard), [701, 503]);
    console.log(
      'PASS public constraints independent unbounded axes and limit reset restore resizing'
    );
    const conflict = create();
    conflict.setMinimumSize(200, 120);
    conflict.setMaximumSize(500, 400);
    const beforeConflict = size(conflict);
    conflict.setMinimumSize(700, 600);
    assert.deepEqual(conflict.getMinimumSize(), [700, 600]);
    assert.deepEqual(conflict.getMaximumSize(), [700, 600]);
    assert.deepEqual(size(conflict), beforeConflict);
    conflict.setMaximumSize(100, 80);
    assert.deepEqual(conflict.getMaximumSize(), [700, 600]);
    assert.deepEqual(size(conflict), beforeConflict);
    conflict.setMinimumSize(0, 0);
    assert.deepEqual(conflict.getMaximumSize(), [100, 80]);
    assert.deepEqual(size(conflict), beforeConflict);
    conflict.setBounds({ width: 320, height: 220 });
    assert.deepEqual(size(conflict), [100, 80]);
    console.log(
      'PASS public constraints minimum wins conflicts remembered maximum and unchanged setter bounds'
    );
    if (process.versions.mite) {
      const invalid = (failure) =>
        failure.code === 'ERR_INVALID_ARG_TYPE' || failure.code === 'ERR_OUT_OF_RANGE';

      for (const value of [-1, 1.5, NaN, Infinity, '200', null, undefined, 16385]) {
        const limits = [standard.getMinimumSize(), standard.getMaximumSize()];
        const bounds = size(standard);
        for (const method of ['setMinimumSize', 'setMaximumSize']) {
          assert.throws(() => standard[method](value, 200), invalid);

          assert.throws(() => standard[method](200, value), invalid);

          assert.deepEqual([standard.getMinimumSize(), standard.getMaximumSize()], limits);
          assert.deepEqual(size(standard), bounds);
        }
        const count = BrowserWindow.getAllWindows().length;
        if (value !== undefined) {
          for (const key of ['minWidth', 'minHeight', 'maxWidth', 'maxHeight']) {
            assert.throws(() => create({ [key]: value }), invalid);

            assert.equal(BrowserWindow.getAllWindows().length, count);
          }
        }
      }
      console.log(
        'PASS Firefly constraints strict bounded inputs fail before native admission or mutation'
      );
    }
    assert.ok(windows.every((window) => !window.isVisible()));

    windows.forEach((window) => window.destroy());

    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    console.error(error);

    windows.forEach((window) => window.destroy());

    app.exit(1);
  }
});
