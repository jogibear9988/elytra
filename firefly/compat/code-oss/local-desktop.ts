/** Explicit experimental Code-OSS application policy; does not implement missing engine APIs. */
type Host = {
  env?: Record<string, string | undefined>;
  versions?: Record<string, string | undefined>;
  _vscodeLibJSLocalDesktopStatus?: unknown;
};
declare const process: Host;
const globals = globalThis as typeof globalThis & { vscode?: { process?: Host } };
// A genuine Node/Electron process takes precedence over any sandbox-shaped global.
const host = typeof process !== 'undefined' ? process : globals.vscode?.process;
const versions = host?.versions;
const libjs =
  versions?.node === undefined &&
  versions?.electron === undefined &&
  typeof versions?.libjs === 'string' &&
  (typeof versions?.mite === 'string' || typeof versions?.firefly === 'string');
const requested = host?.env?.ELYTRA_CODE_OSS_LOCAL_DESKTOP;
if (libjs && requested !== undefined && requested !== '0' && requested !== '1') {
  throw Object.assign(new Error('ELYTRA_CODE_OSS_LOCAL_DESKTOP must be 0 or 1'), {
    code: 'ERR_CODE_OSS_LOCAL_DESKTOP_CONFIG'
  });
}
const enabled = Boolean(libjs && requested === '1');
const status = Object.freeze({
  enabled,
  scope: 'experimental-local-desktop',
  workbench: 'not-accepted',
  disabled: Object.freeze(['webviews', 'remote', 'display-capture', 'engine-code-cache'])
});
if (enabled && typeof versions?.mite === 'string' && host) {
  Object.defineProperty(host, '_vscodeLibJSLocalDesktopStatus', {
    value: status,
    enumerable: true
  });
  console.warn(
    'Elytra experimental local desktop: webviews, remote resources, display capture and engine code caches are disabled; workbench not yet accepted.'
  );
}
export function isLocalDesktop(): boolean {
  return enabled;
}
export function localDesktopStatus() {
  return status;
}
export function assertLocalDesktopFeature(feature: string): void {
  if (enabled) {
    throw Object.assign(new Error(`Elytra local desktop does not support ${feature}`), {
      code: 'ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED'
    });
  }
}
export function rejectLocalDesktopFeature(feature: string): Promise<never> | undefined {
  if (!enabled) return undefined;
  try {
    assertLocalDesktopFeature(feature);
  } catch (error) {
    return Promise.reject(error);
  }
  throw new Error('Unreachable local desktop policy');
}
export function assertLocalDesktopCLI(options: Record<string, unknown>): void {
  if (!enabled) return;
  if (options.remote !== undefined) assertLocalDesktopFeature('remote command line');
  for (const key of ['_', 'file-uri', 'folder-uri']) {
    const values = Array.isArray(options[key]) ? (options[key] as unknown[]) : [options[key]];
    if (
      values.some(
        (value) =>
          typeof value === 'string' &&
          /^vscode-(remote|remote-resource|managed-remote-resource):/i.test(value)
      )
    ) {
      assertLocalDesktopFeature('remote resource command line');
    }
  }
}
export function localDesktopCodeCache(original: () => string | undefined): string | undefined {
  return enabled ? undefined : original();
}
type Scheme = { scheme: string; privileges: Record<string, boolean> };
export function localDesktopSchemes(original: Scheme[]): Scheme[] {
  if (!enabled) return original;
  const names = [
    'vscode-webview',
    'vscode-file',
    'vscode-remote-resource',
    'vscode-managed-remote-resource'
  ];
  const keys = [
    ['standard', 'secure', 'supportFetchAPI', 'corsEnabled', 'allowServiceWorkers', 'codeCache'],
    ['standard', 'secure', 'supportFetchAPI', 'corsEnabled', 'codeCache'],
    ['secure', 'supportFetchAPI', 'corsEnabled'],
    ['secure', 'supportFetchAPI', 'corsEnabled']
  ];
  if (
    original.length !== 4 ||
    original.some(
      (item, index) =>
        item.scheme !== names[index] ||
        Object.keys(item).length !== 2 ||
        Object.keys(item.privileges).length !== keys[index].length ||
        keys[index].some((key) => item.privileges[key] !== true)
    )
  ) {
    throw Object.assign(
      new Error('Code-OSS scheme declarations differ from the audited local desktop policy'),
      { code: 'ERR_CODE_OSS_LOCAL_DESKTOP_CONTRACT' }
    );
  }
  return [
    {
      scheme: 'vscode-file',
      privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true }
    }
  ];
}
