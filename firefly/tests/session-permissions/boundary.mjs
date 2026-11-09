/** Native-only component: the document never calls permission or device APIs. */
import { app, BrowserWindow, session } from 'firefly';
import fs from 'node:fs';
import path from 'node:path';

const report = process.env.ELYTRA_PERMISSION_BOUNDARY_REPORT;
if (!report) {
  throw Error('Boundary driver requires an owned report path');
}
app.setPath('userData', path.join(import.meta.dirname, 'boundary-config'));

app.on('window-all-closed', () => {});

await app.whenReady();
let calls = 0;
let requests = 0;
let denials = 0;
let phase = 0;

session.defaultSession.setPermissionCheckHandler(() => false);

session.defaultSession.setPermissionCheckHandler((contents, permission, origin, details) => {
  calls++;
  if (
    contents !== window.webContents ||
    permission !== 'geolocation' ||
    typeof origin !== 'string' ||
    !details.isMainFrame
  ) {
    throw Error('Native boundary metadata lost actual contents identity');
  }
  return true;
});

const window = new BrowserWindow({ show: false });
await window.loadFile(path.join(import.meta.dirname, 'boundary.html'));
let result;
for (let attempt = 0; attempt < 2200; attempt++) {
  if (fs.existsSync(report)) {
    result = JSON.parse(fs.readFileSync(report, 'utf8'));
    if (!result.passed) {
      throw Error('Native permission component failed: ' + JSON.stringify(result));
    }
    if (result.stage === 'identity' && phase === 0) {
      if (calls !== 1 || result.cases !== 7) {
        throw Error('Forged identities reached the application handler');
      }
      phase = 1;

      session.defaultSession.setPermissionRequestHandler(
        (contents, permission, callback, details) => {
          requests++;
          if (
            contents !== window.webContents ||
            permission !== 'geolocation' ||
            !details.isMainFrame
          ) {
            throw Error('Native request metadata lost actual contents identity');
          }
          callback(true);
        }
      );
    } else if (result.stage === 'granted' && phase === 1) {
      if (requests !== 1 || calls !== 1) {
        throw Error('Native grant component dispatched unexpected application callbacks');
      }
      phase = 2;

      session.defaultSession.setPermissionCheckHandler((contents, permission) => {
        denials++;
        if (contents !== window.webContents || permission !== 'geolocation') {
          throw Error('Revocation metadata changed');
        }
        return false;
      });
    } else if (result.stage === 'done') {
      break;
    }
  }
  await new Promise((resolve) => setTimeout(resolve, 10));
}
if (
  !result?.passed ||
  result.stage !== 'done' ||
  result.cases !== 12 ||
  calls !== 1 ||
  requests !== 1 ||
  denials !== 1
) {
  throw Error(
    'Native permission boundary failed: ' + JSON.stringify({ result, calls, requests, denials })
  );
}
console.log('PASS exact native permission boundary and controlled grant revocation twelve cases');
window.destroy();
app.exit(0);
