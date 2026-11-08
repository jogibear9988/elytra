/** Exercise the actual Code-OSS argv/config/userEnv pattern from a restricted preload. */
import { contextBridge, ipcRenderer } from 'firefly';
const id = process.argv.find((value) => value.startsWith('--vscode-window-config=')).split('=')[1];
const originalEnv = process.env.FIREFLY_PROCESS_FIXTURE;
const config = ipcRenderer.invoke('vscode:window-config', id).then((value) => {
  Object.assign(process.env, value.userEnv);
  return value.id;
});
process.env.FIREFLY_PROCESS_FIXTURE = 'local-' + id;
process.argv.push('preload-local');
const argv = process.argv.slice(0, -1);
let unsupported = 0;
for (const invoke of [() => process.on('loaded', () => {}), () => process.getProcessMemoryInfo()]) {
  try {
    invoke();
  } catch (error) {
    if (error.code === 'ERR_FIREFLY_UNSUPPORTED_API') {
      unsupported++;
    }
  }
}
let nativeExportDenied = false;
try {
  contextBridge.exposeInMainWorld('rawProcessNative', { on: process.on });
} catch {
  nativeExportDenied = true;
}
contextBridge.exposeInMainWorld('processFixture', {
  get platform() {
    return process.platform;
  },

  get arch() {
    return process.arch;
  },

  get pid() {
    return process.pid;
  },

  get execPath() {
    return process.execPath;
  },

  get version() {
    return process.version;
  },

  get versions() {
    return process.versions;
  },

  argv,
  originalEnv,
  flags:
    process.sandboxed === true &&
    process.contextIsolated === true &&
    process.isMainFrame === true &&
    process.type === 'renderer',
  unsupported: unsupported === 2,
  nativeExportDenied,

  async report(value) {
    const configuredId = await config;
    return ipcRenderer.invoke('process:report', {
      ...value,
      config: configuredId === id,
      localEnv:
        process.env.FIREFLY_PROCESS_FIXTURE === 'local-' + id &&
        process.env.FIREFLY_USER_ENV === 'merged'
    });
  }
});
