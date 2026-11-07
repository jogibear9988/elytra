/** Restricted preload imports share one frozen facade; they never gain main APIs. */
const facade = require('firefly');
let enabled = false;
let shared = false;
try {
  const alias = require('electron');
  enabled = true;
  shared = alias === facade && Object.isFrozen(alias) && alias.app === undefined;
} catch (error) {
  if (error.code !== 'ERR_FIREFLY_UNSUPPORTED_API') {
    throw error;
  }
}
let denied = 0;
for (const name of [
  'fs',
  'node:fs',
  'original-fs',
  'child_process',
  'electron/main',
  'electron/renderer'
]) {
  try {
    require(name);
  } catch (error) {
    if (error.code === 'ERR_FIREFLY_UNSUPPORTED_API') {
      ++denied;
    }
  }
}
facade.contextBridge.exposeInMainWorld('audit', {
  enabled,
  shared,
  denied,

  later() {
    try {
      return require('electron') === facade;
    } catch (error) {
      return error.code === 'ERR_FIREFLY_UNSUPPORTED_API' ? false : undefined;
    }
  }
});
