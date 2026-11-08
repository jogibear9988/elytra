/** Code-OSS-shaped metadata getters are read once from this trusted source realm. */
import { contextBridge, ipcRenderer } from 'firefly';
let reads = 0;
let sideEffects = 0;
let failed = false;
let resultReads = 0;
const state = { env: { LABEL: 'initial' }, versions: { engine: 'fixture' } };

const fn = () => 42;

try {
  contextBridge.exposeInMainWorld('getterFailed', {
    good: () => 1,

    get bad() {
      sideEffects++;
      throw Error('source-only-getter-error');
    }
  });
} catch {
  failed = true;
}
contextBridge.exposeInMainWorld('getterApi', {
  process: {
    get platform() {
      reads++;
      return 'fixture';
    },

    get arch() {
      reads++;
      return 'fixture-arch';
    },

    get env() {
      reads++;
      return { ...state.env };
    },

    get versions() {
      reads++;
      return state.versions;
    },

    get type() {
      reads++;
      return 'renderer';
    },

    get execPath() {
      reads++;
      return '/fixture/firefly';
    }
  },

  get action() {
    return fn;
  },

  alias: fn,

  get receiver() {
    return this.alias === fn;
  },

  update() {
    state.env.LABEL = 'later';
  },

  status() {
    return { reads, sideEffects, failed, resultReads };
  },

  echo(value) {
    return value;
  },

  result() {
    return {
      get value() {
        resultReads++;
        return 1;
      }
    };
  },

  report(value) {
    return ipcRenderer.invoke('getter:report', value);
  }
});
