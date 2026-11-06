/** Shared Electron/Firefly native override and renderer CSS acceptance, with hidden windows. */
import { app, BrowserWindow, nativeTheme } from 'firefly';
const original = nativeTheme.themeSource;

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function appearance(window, dark) {
  const expected = dark ? 'dark' : 'light';
  for (let i = 0; i < 200; i++) {
    if (window.webContents.getTitle() === expected) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error('Missing CSS native theme ' + expected + ': ' + window.webContents.getTitle());
}

app.whenReady().then(async () => {
  try {
    let updates = 0;

    nativeTheme.on('updated', () => {
      updates++;
    });

    nativeTheme.themeSource = 'dark';
    const first = new BrowserWindow({ show: false });
    await first.loadFile('theme.html');
    await appearance(first, true);
    for (const source of ['light', 'dark', 'system']) {
      nativeTheme.themeSource = source;
      check(nativeTheme.themeSource === source, 'native source assignment');
      const expected = source === 'system' ? nativeTheme.shouldUseDarkColors : source === 'dark';
      check(nativeTheme.shouldUseDarkColors === expected, 'computed native selection');
      await appearance(first, expected);
    }
    nativeTheme.themeSource = 'light';
    const second = new BrowserWindow({ show: false });
    await second.loadFile('theme.html');
    await appearance(first, false);
    await appearance(second, false);
    nativeTheme.themeSource = 'dark';
    await appearance(first, true);
    await appearance(second, true);
    check(updates >= 2, 'native update delivery');
    for (const key of [
      'shouldUseHighContrastColors',
      'shouldUseInvertedColorScheme',
      'prefersReducedTransparency',
      'shouldDifferentiateWithoutColor'
    ]) {
      check(typeof nativeTheme[key] === 'boolean', 'native accessibility flag ' + key);
    }
    nativeTheme.themeSource = original;
    first.destroy();
    second.destroy();
    console.log(
      JSON.stringify({ theme: true, css: true, initial: true, existingAndNewWindows: true })
    );
    app.quit();
  } catch (error) {
    console.error(error.stack);
    nativeTheme.themeSource = original;
    app.exit(1);
  }
});
