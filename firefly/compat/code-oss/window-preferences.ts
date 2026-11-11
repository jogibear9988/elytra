/** Application-only omission of an explicitly disabled V8/Blink cache policy. */
type Diagnostic = {
  engine: string;
  scope: string;
  cache: string;
  omittedWindowPreferences: readonly { name: string; value: string; reason: string }[];
};
type Host = {
  versions: Record<string, string | undefined>;
  env: Record<string, string | undefined>;
  execPath: string;
  _vscodeLibJSLocalDesktopStatus?: {
    enabled?: boolean;
    scope?: string;
    disabled?: readonly string[];
  };
  _vscodeLibJSEngineStatus?: { engine?: string; scope?: string };
  _vscodeLibJSWindowPreferencesStatus?: Diagnostic;
  _vscodeLibJSOffscreenDiagnosticStatus?: {
    scope: string;
    desktopAccepted: boolean;
    omittedInputPolicy: string;
    reason: string;
  };
  _vscodeLibJSTouchBarStatus?: {
    engine: string;
    scope: string;
    enabled: boolean;
    reason: string;
  };
  _vscodeLibJSLegacyPasteStatus?: {
    engine: string;
    scope: string;
    omittedWindowPreferences: readonly { name: string; value: boolean; reason: string }[];
  };
};
const host = process as typeof process & Host;
let published: Diagnostic | undefined;
let pastePublished: Host['_vscodeLibJSLegacyPasteStatus'];
let touchBarPublished: Host['_vscodeLibJSTouchBarStatus'];
let offscreenPublished: Host['_vscodeLibJSOffscreenDiagnosticStatus'];

function refuse(): never {
  throw Object.assign(new Error('Code-OSS cannot apply this V8 window cache policy on LibJS'), {
    code: 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
  });
}

/** Keep ordinary Electron's original value, including its cache-enabled branch. */
export function libJSWindowCachePreferences(value: string): { v8CacheOptions: string } | {} {
  const versions = host.versions;
  if (typeof versions.node === 'string' || typeof versions.electron === 'string') {
    return { v8CacheOptions: value };
  }
  const local = host._vscodeLibJSLocalDesktopStatus;
  const engine = host._vscodeLibJSEngineStatus;
  if (
    typeof versions.mite !== 'string' ||
    typeof versions.libjs !== 'string' ||
    value !== 'none' ||
    host.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP !== '1' ||
    host.env.VSCODE_CODE_CACHE_PATH ||
    local?.enabled !== true ||
    local.scope !== 'experimental-local-desktop' ||
    !Array.isArray(local.disabled) ||
    !local.disabled.includes('engine-code-cache') ||
    engine?.engine !== 'LibJS/LibWeb' ||
    engine.scope !== 'implicit-code-oss-engine-tuning-only'
  ) {
    refuse();
  }
  if (!published) {
    // This status describes one omitted application preference, never an engine
    // cache implementation. Repeated windows reuse the same immutable record.
    if (Object.hasOwn(host, '_vscodeLibJSWindowPreferencesStatus')) {
      refuse();
    }
    published = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-disabled-v8-window-cache-only',
      cache: 'unavailable',
      omittedWindowPreferences: Object.freeze([
        Object.freeze({
          name: 'v8CacheOptions',
          value,
          reason: 'V8/Blink code caches do not configure LibJS; the local desktop cache is disabled'
        })
      ])
    });
    Object.defineProperty(host, '_vscodeLibJSWindowPreferencesStatus', {
      value: published,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return {};
}

function verifiedLocalWindowAdaptation(): boolean {
  const versions = host.versions;
  const local = host._vscodeLibJSLocalDesktopStatus;
  const engine = host._vscodeLibJSEngineStatus;
  return (
    typeof versions.mite === 'string' &&
    typeof versions.libjs === 'string' &&
    host.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP === '1' &&
    !host.env.VSCODE_CODE_CACHE_PATH &&
    local?.enabled === true &&
    local.scope === 'experimental-local-desktop' &&
    Array.isArray(local.disabled) &&
    local.disabled.includes('engine-code-cache') &&
    engine?.engine === 'LibJS/LibWeb' &&
    engine.scope === 'implicit-code-oss-engine-tuning-only'
  );
}

/** Omit only the pinned app's legacy synchronous script-paste opt-in.
 * Native UI paste is a separate future capability; no clipboard fallback is claimed.
 */
export function libJSWindowPastePreferences(
  value: boolean
): { enableDeprecatedPaste: boolean } | {} {
  const versions = host.versions;
  if (typeof versions.node === 'string' || typeof versions.electron === 'string') {
    return { enableDeprecatedPaste: value };
  }
  if (value !== true || !verifiedLocalWindowAdaptation()) {
    throw Object.assign(
      new Error('Code-OSS deprecated synchronous script paste is unavailable on LibWeb'),
      {
        code: 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
      }
    );
  }
  if (!pastePublished) {
    if (Object.hasOwn(host, '_vscodeLibJSLegacyPasteStatus')) {
      throw Object.assign(new Error('Conflicting Code-OSS script-paste omission status'), {
        code: 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
      });
    }
    pastePublished = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-deprecated-sync-paste-omission-only',
      omittedWindowPreferences: Object.freeze([
        Object.freeze({
          name: 'enableDeprecatedPaste',
          value: true,
          reason:
            'Deprecated synchronous execCommand paste is unavailable; native webContents.paste still needs integration'
        })
      ])
    });
    Object.defineProperty(host, '_vscodeLibJSLegacyPasteStatus', {
      value: pastePublished,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return {};
}

/** Disable this optional app integration without introducing a fake TouchBar API. */
export function skipLibJSTouchBar(): boolean {
  const versions = host.versions;
  if (typeof versions.node === 'string' || typeof versions.electron === 'string') {
    return false;
  }
  if (!verifiedLocalWindowAdaptation()) {
    refuse();
  }
  if (!touchBarPublished) {
    if (Object.hasOwn(host, '_vscodeLibJSTouchBarStatus')) {
      refuse();
    }
    touchBarPublished = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-local-touchbar-opt-out-only',
      enabled: false,
      reason:
        'Optional Code-OSS TouchBar initialization is disabled; Firefly TouchBar remains unsupported'
    });
    Object.defineProperty(host, '_vscodeLibJSTouchBarStatus', {
      value: touchBarPublished,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return true;
}

/** An unexpected feature update must fail explicitly before touching group state. */
export function assertLibJSTouchBarSupported(): void {
  const versions = host.versions;
  if (typeof versions.node === 'string' || typeof versions.electron === 'string') {
    return;
  }
  if (!verifiedLocalWindowAdaptation()) {
    refuse();
  }
  throw Object.assign(new Error('TouchBar is disabled in the Code-OSS local desktop experiment'), {
    code: 'ERR_CODE_OSS_LIBJS_TOUCHBAR_UNAVAILABLE'
  });
}

/** Test-only omission for the exact offscreen frontend; production input policy stays intact. */
export function libJSFirstMousePreferences(value: boolean): { acceptFirstMouse: boolean } | {} {
  if (typeof host.versions.node === 'string' || typeof host.versions.electron === 'string') {
    return { acceptFirstMouse: value };
  }
  const diagnostic = host.env.ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC;
  if (diagnostic === undefined) {
    return { acceptFirstMouse: value };
  }
  if (
    diagnostic !== '1' ||
    host.env.QT_QPA_PLATFORM !== 'offscreen' ||
    host.execPath.split(/[\\/]/).pop() !== 'firefly-view-offscreen' ||
    typeof value !== 'boolean' ||
    !verifiedLocalWindowAdaptation()
  ) {
    throw Object.assign(
      new Error('Code-OSS offscreen diagnostic requires the verified test-only frontend'),
      {
        code: 'ERR_CODE_OSS_LIBJS_OFFSCREEN_DIAGNOSTIC'
      }
    );
  }
  if (!offscreenPublished) {
    if (Object.hasOwn(host, '_vscodeLibJSOffscreenDiagnosticStatus')) {
      throw Object.assign(new Error('Conflicting Code-OSS offscreen diagnostic status'), {
        code: 'ERR_CODE_OSS_LIBJS_OFFSCREEN_DIAGNOSTIC'
      });
    }
    offscreenPublished = Object.freeze({
      scope: 'code-oss-test-only-offscreen-diagnostic',
      desktopAccepted: false,
      omittedInputPolicy: 'acceptFirstMouse',
      reason:
        'The offscreen diagnostic has no native desktop first-mouse input; production input policy remains unchanged'
    });
    Object.defineProperty(host, '_vscodeLibJSOffscreenDiagnosticStatus', {
      value: offscreenPublished,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return {};
}
