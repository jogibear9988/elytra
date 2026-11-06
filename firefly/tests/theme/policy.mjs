/** Exercise the actual API factory with deterministic system snapshots, without GUI. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';
import { readFileSync } from 'node:fs';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

let system = {
  dark: false,
  shouldUseHighContrastColors: false,
  shouldUseInvertedColorScheme: false,
  prefersReducedTransparency: false,
  shouldDifferentiateWithoutColor: false
};
let handler;
const calls = [];
let fail = false;
const native = {
  appPath: process.cwd(),

  setHandler(value) {
    handler = value;
  },

  themeSnapshot() {
    return JSON.stringify(system);
  },

  themeSetSource(value) {
    if (fail) {
      throw new Error('native fixture refused');
    }
    calls.push(value);
  }
};
const { app, nativeTheme } = createApi(native, EventEmitter, path, urls);
check(!app.isReady() && nativeTheme instanceof EventEmitter, 'main-process singleton');
check(nativeTheme.themeSource === 'system' && !nativeTheme.shouldUseDarkColors, 'system default');
let updated = 0;

nativeTheme.on('updated', () => {
  updated++;
  check(typeof nativeTheme.shouldUseDarkColors === 'boolean', 'reentrant event read');
});

nativeTheme.themeSource = 'dark';
check(nativeTheme.shouldUseDarkColors && !system.dark, 'override independent from OS');
check(nativeTheme.shouldUseDarkColorsForSystemIntegratedUI, 'macOS integrated UI policy');
check(updated === 0, 'native notifications queued');
handler(0, 'theme-changed', '');
handler(0, 'theme-changed', '');
check(updated === 1, 'change coalescing');
nativeTheme.themeSource = 'dark';
check(calls.length === 1, 'same source is a no-op');
nativeTheme.themeSource = 'light';
handler(0, 'theme-changed', '');
check(!nativeTheme.shouldUseDarkColors && updated === 2, 'explicit light');
nativeTheme.themeSource = 'system';
handler(0, 'theme-changed', '');
check(updated === 3 && JSON.stringify(calls) === '["dark","light","system"]', 'source changes');
console.log('PASS theme source guards and native override boundary before readiness');

system.dark = true;
system.shouldUseHighContrastColors = true;
system.shouldUseInvertedColorScheme = true;
system.prefersReducedTransparency = true;
system.shouldDifferentiateWithoutColor = true;
check(
  nativeTheme.shouldUseDarkColors && nativeTheme.prefersReducedTransparency,
  'fresh system observations'
);
handler(0, 'theme-changed', '');
check(
  updated === 4 &&
    nativeTheme.shouldUseHighContrastColors &&
    nativeTheme.shouldUseInvertedColorScheme &&
    nativeTheme.shouldDifferentiateWithoutColor,
  'system event flags'
);
handler(0, 'theme-changed', '');
check(updated === 4, 'unchanged system notification');
console.log('PASS theme system accessibility updates and reentrant baseline ownership');

for (const value of [undefined, null, 1, {}, 'auto', 'DARK']) {
  let code;
  try {
    nativeTheme.themeSource = value;
  } catch (error) {
    code = error.code;
  }
  check(code === 'ERR_INVALID_ARG_VALUE' && nativeTheme.themeSource === 'system', 'invalid source');
}
let readonly = false;
try {
  nativeTheme.shouldUseDarkColors = false;
} catch {
  readonly = true;
}
check(readonly && nativeTheme.shouldUseDarkColors, 'readonly computed state');
fail = true;
let refused = false;
try {
  nativeTheme.themeSource = 'light';
} catch {
  refused = true;
}
check(refused && nativeTheme.themeSource === 'system', 'native failure does not commit source');
console.log('PASS theme readonly properties invalid source and failed native update');

if (process.argv[2]) {
  system = JSON.parse(readFileSync(process.argv[2], 'utf8'));
  check(nativeTheme.shouldUseDarkColors === system.dark, 'actual native dark selection');
  for (const key of Object.keys(system).filter((key) => key !== 'dark')) {
    check(
      nativeTheme[key] === system[key] && typeof nativeTheme[key] === 'boolean',
      'actual native flag ' + key
    );
  }
  console.log('PASS actual macOS theme snapshot through Node/Mite API factory');
}
console.log('DONE theme policy');
