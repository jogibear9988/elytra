/** Print the desktop layout without creating a BrowserWindow. */
import { app, screen } from 'firefly';

await app.whenReady();
console.log(
  JSON.stringify(
    {
      primary: screen.getPrimaryDisplay(),
      displays: screen.getAllDisplays(),
      cursor: screen.getCursorScreenPoint()
    },
    null,
    2
  )
);
app.quit();
