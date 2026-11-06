/** Actual native outer/content geometry, shared by Electron and Firefly using hidden windows. */
import { app, BrowserWindow, screen } from 'firefly';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function equal(actual, expected, message) {
  const normalize = (value) =>
    Array.isArray(value) ? value : Object.fromEntries(Object.entries(value).sort());

  check(
    JSON.stringify(normalize(actual)) === JSON.stringify(normalize(expected)),
    message + ': ' + JSON.stringify(actual)
  );
}

app.whenReady().then(async () => {
  try {
    const { workArea: area } = screen.getPrimaryDisplay();
    const x = area.x + 40;
    const y = area.y + 40;
    const window = new BrowserWindow({ x, y, width: 600, height: 400, show: false });
    equal(window.getBounds(), { x, y, width: 600, height: 400 }, 'initial outer rectangle');
    const content = window.getContentBounds();
    await window.loadFile('theme.html');
    check(content.width <= 600 && content.height < 400, 'real decorated frame margins');
    equal(window.getContentSize(), [content.width, content.height], 'content size');
    let moves = 0;

    window.on('move', () => {
      moves++;
    });

    window.setBounds({ x: x + 20, width: 620 });
    equal(
      window.getBounds(),
      { x: x + 20, y, width: 620, height: 400 },
      'partial native rectangle'
    );
    window.setContentSize(500, 300);
    equal(window.getContentSize(), [500, 300], 'native content resize');
    window.setContentBounds({ x: x + 30, y: y + 50, width: 500, height: 300 });
    const movedContent = window.getContentBounds();
    check(movedContent.x === x + 30 && movedContent.y === y + 50, 'native content positioning');
    window.setSize(600, 400);
    equal(window.getSize(), [600, 400], 'native outer resize');
    window.setPosition(x, y);
    equal(window.getPosition(), [x, y], 'native outer position');
    window.center();
    const centered = window.getBounds();
    const work = screen.getDisplayMatching(centered).workArea;
    check(
      Math.abs(centered.x - (work.x + (work.width - centered.width) / 2)) <= 1 &&
        centered.y >= work.y &&
        centered.y + centered.height <= work.y + work.height,
      'native AppKit center within work area'
    );
    const sized = new BrowserWindow({ width: 400, height: 300, useContentSize: true, show: false });
    equal(sized.getContentSize(), [400, 300], 'native content-sized construction');
    await sized.loadFile('theme.html');
    const frameless = new BrowserWindow({ width: 400, height: 300, frame: false, show: false });
    equal(frameless.getSize(), frameless.getContentSize(), 'native frameless dimensions');
    await frameless.loadFile('theme.html');
    await new Promise((resolve) => setTimeout(resolve, 40));
    check(moves > 0, 'native move event delivery');
    window.destroy();
    sized.destroy();
    frameless.destroy();
    console.log(
      JSON.stringify({
        geometry: true,
        decorated: true,
        content: true,
        frameless: true,
        move: true,
        center: [centered.x - work.x, centered.y - work.y]
      })
    );
    app.quit();
  } catch (error) {
    console.error(error.stack);
    app.exit(1);
  }
});
