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
  _vscodeLibJSLocalDesktopStatus?: {
    enabled?: boolean;
    scope?: string;
    disabled?: readonly string[];
  };
  _vscodeLibJSEngineStatus?: { engine?: string; scope?: string };
  _vscodeLibJSWindowPreferencesStatus?: Diagnostic;
};
const host = process as typeof process & Host;
let published: Diagnostic | undefined;

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
