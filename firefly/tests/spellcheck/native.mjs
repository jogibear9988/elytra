/** Controlled native provider admission; no page navigation, spelling RPC or visible windows. */
import { app, BrowserWindow, WebContentsView } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
const timeout = setTimeout(() => {
  console.error('Native spellcheck admission probe timed out');
  app.exit(1);
}, 15000);

await app.whenReady();
let control;
let initialView;
let restoredWindow;
let restoredView;
try {
  assert.equal(process.env.QT_QPA_PLATFORM, 'offscreen');
  control = new BrowserWindow({ show: false, webPreferences: { spellcheck: false } });
  initialView = new WebContentsView({ webPreferences: { spellcheck: false } });

  async function handshake(stage) {
    const title = `elytra-spellcheck-${stage}`;
    const expected = `${title}-ok`;
    const deadline = Date.now() + 2500;
    control.setTitle(title);
    while (control.getTitle() !== expected) {
      assert.notEqual(control.getTitle(), 'elytra-spellcheck-failed', `Native ${stage} failed`);
      assert.ok(Date.now() < deadline, `Native ${stage} did not acknowledge`);
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  }

  await handshake('baseline');
  await handshake('install');
  const unsupported = { code: 'ERR_FIREFLY_UNSUPPORTED_API' };

  assert.throws(
    () => new BrowserWindow({ show: false, webPreferences: { spellcheck: false } }),
    unsupported
  );

  assert.throws(() => new WebContentsView({ webPreferences: { spellcheck: false } }), unsupported);
  // Omission must not bypass the independent native factory invariant.
  assert.throws(() => new BrowserWindow({ show: false }), unsupported);

  assert.throws(() => new WebContentsView(), unsupported);

  await handshake('blocked');
  await handshake('reset');
  restoredWindow = new BrowserWindow({ show: false, webPreferences: { spellcheck: false } });
  restoredView = new WebContentsView({ webPreferences: { spellcheck: false } });
  await handshake('restored');
  assert.equal(control.isVisible(), false);
  assert.equal(restoredWindow.isVisible(), false);
  console.log('PASS native spellcheck provider-null invariant and restored public admission');
} catch (error) {
  console.error(error);
  clearTimeout(timeout);
  app.exit(1);
}

restoredView?.webContents.close({ waitForBeforeUnload: false });
initialView?.webContents.close({ waitForBeforeUnload: false });
restoredWindow?.destroy();
control?.destroy();
clearTimeout(timeout);
app.exit(0);
