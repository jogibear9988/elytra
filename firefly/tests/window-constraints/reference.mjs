/** One hidden pinned Electron oracle; no mouse input or visible desktop windows. */
import { app, BrowserWindow } from 'electron';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const deadline = setTimeout(() => app.exit(1), 15000);

app.whenReady().then(() => {
  const windows = [];
  try {
    const result = {};

    function create(options = {}) {
      const window = new BrowserWindow({ show: false, width: 320, height: 220, ...options });
      windows.push(window);
      return window;
    }

    function state(window) {
      return {
        minimum: window.getMinimumSize(),
        maximum: window.getMaximumSize(),
        bounds: window.getBounds(),
        content: window.getContentBounds()
      };
    }

    const outer = create();
    result.defaults = state(outer);
    result.setters = [];
    for (const [method, width, height] of [
      ['setMinimumSize', 200, 120],
      ['setMaximumSize', 500, 400],
      ['setMinimumSize', 700, 600],
      ['setMaximumSize', 100, 80],
      ['setMinimumSize', 0, 0],
      ['setMaximumSize', 0, 0],
      ['setMaximumSize', 0, 350],
      ['setMaximumSize', 450, 0]
    ]) {
      outer[method](width, height);
      result.setters.push({ method, width, height, state: state(outer) });
    }
    result.badTypes = [];
    for (const [width, height] of [
      [-1, 10],
      [1.5, 10],
      [NaN, 10],
      [Infinity, 10],
      ['200', 10],
      [undefined, 10],
      [null, 10],
      [200, undefined],
      [-0, 10],
      [2147483648, 10]
    ]) {
      try {
        outer.setMinimumSize(width, height);
        result.badTypes.push({ width: String(width), height: String(height), state: state(outer) });
      } catch (error) {
        result.badTypes.push({
          width: String(width),
          height: String(height),
          name: error.name,
          message: error.message
        });
      }
    }
    result.constructors = [];
    for (const options of [
      { minWidth: 200, minHeight: 120, maxWidth: 500, maxHeight: 400 },
      { width: 100, height: 80, minWidth: 200, minHeight: 120 },
      { width: 700, height: 600, maxWidth: 500, maxHeight: 400 },
      { minWidth: 500, minHeight: 400, maxWidth: 200, maxHeight: 120 },
      { useContentSize: true, minWidth: 200, minHeight: 120, maxWidth: 500, maxHeight: 400 },
      { maxWidth: 0, maxHeight: 350 },
      { minWidth: -1, minHeight: 1.5 },
      { minWidth: '200', maxHeight: '350' }
    ]) {
      try {
        result.constructors.push({ options, state: state(create(options)) });
      } catch (error) {
        result.constructors.push({ options, name: error.name, message: error.message });
      }
    }
    const constrained = create({ minWidth: 200, minHeight: 120, maxWidth: 500, maxHeight: 400 });
    result.resize = [];
    for (const bounds of [
      { width: 100, height: 80 },
      { width: 900, height: 700 },
      { width: 320, height: 220 }
    ]) {
      constrained.setBounds(bounds);
      result.resize.push({ requested: bounds, actual: state(constrained) });
    }
    console.log(JSON.stringify(result));

    windows.forEach((window) => window.destroy());

    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    console.error(error);

    windows.forEach((window) => window.destroy());

    app.exit(1);
  }
});
