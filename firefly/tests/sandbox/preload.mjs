/** Authored ESM, bundled to the supported isolated CJS preload format by the test. */
import * as api from 'firefly';
api.contextBridge.exposeInMainWorld('sandboxFixture', {
  isolated: true,
  mainMissing: api.app === undefined,
  processMissing: typeof process === 'undefined'
});
