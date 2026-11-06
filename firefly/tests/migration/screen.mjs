/** Hosted desktop acceptance: query actual screen exports without creating a window. */
import { app, screen } from 'firefly';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

app.whenReady().then(async () => {
  try {
    const displays = screen.getAllDisplays();
    const primary = screen.getPrimaryDisplay();
    check(displays.length > 0 && displays.some((d) => d.id === primary.id), 'primary display');
    for (const d of displays) {
      check(Number.isInteger(d.id) && d.scaleFactor > 0, 'display metadata');
      check(d.bounds.width > 0 && d.bounds.height > 0, 'positive bounds');
      check(d.size.width === d.bounds.width && d.size.height === d.bounds.height, 'DIP size');
      check(d.workAreaSize.width === d.workArea.width, 'work area width');
      check(d.workAreaSize.height === d.workArea.height, 'work area height');
      check(screen.getDisplayMatching(d.bounds).id === d.id, 'matching native display');
    }
    const cursor = screen.getCursorScreenPoint();
    check(Number.isInteger(cursor.x) && Number.isInteger(cursor.y), 'cursor DIP point');
    check(
      displays.some((d) => d.id === screen.getDisplayNearestPoint(cursor).id),
      'nearest cursor'
    );
    const copy = screen.getPrimaryDisplay();
    copy.bounds.width = -1;
    check(screen.getPrimaryDisplay().bounds.width > 0, 'isolated display snapshots');
    console.log(JSON.stringify({ screen: true, displays: displays.length }));
    app.quit();
  } catch (error) {
    console.error(error.stack);
    app.exit(1);
  }
});
