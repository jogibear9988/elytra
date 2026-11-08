/** Persistent profile settings must agree with the new main's default theme source. */
import { app, nativeTheme } from 'firefly';
import fs from 'node:fs';
import path from 'node:path';
app.setPath('sessionData', process.argv[2]);
if (process.argv[3] === 'dark') {
  nativeTheme.themeSource = 'dark';
}
await app.whenReady();
const settings = JSON.parse(
  fs.readFileSync(path.join(process.argv[2], 'config/Settings.json'), 'utf8')
);
console.log(
  JSON.stringify({ themeSource: nativeTheme.themeSource, appearance: settings.appearance })
);
app.exit(0);
