/** Real external module graph: no bundling, inline substitution or privileged loader. */
import * as counter from './counter.mjs';
import { value } from './space ü.mjs';

try {
  await Promise.resolve();
  const again = await import('./counter.mjs');
  counter.increment();
  if (
    !classicLoaded ||
    value !== 42 ||
    again !== counter ||
    again.count !== 1 ||
    counterEvaluations !== 1 ||
    !import.meta.url.endsWith('/renderer.mjs')
  ) {
    throw Error('Module values, live bindings or identity failed');
  }
  for (const name of ['node:fs', 'firefly', 'an-uninstalled-package', './missing.mjs']) {
    let denied = false;
    try {
      await import(name);
    } catch {
      denied = true;
    }
    if (!denied) {
      throw Error('Renderer imported ' + name);
    }
  }
  if (
    typeof require !== 'undefined' ||
    typeof process !== 'undefined' ||
    typeof Buffer !== 'undefined'
  ) {
    throw Error('Native capability appeared in renderer');
  }
  document.title = 'MODULES_OK';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
