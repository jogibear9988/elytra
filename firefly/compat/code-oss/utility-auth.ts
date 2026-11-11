/** Optional app integration only: Firefly does not forward utility HTTP authentication. */
type Status = {
  engine: string;
  scope: string;
  available: boolean;
  requested: boolean;
  reason: string;
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
  _vscodeLibJSUtilityAuthStatus?: Status;
  _vscodeLibJSUtilityUnsignedLibrariesStatus?: Status;
};
const host = process as typeof process & Host;
let published: Status | undefined;
let unsignedPublished: Status | undefined;

/** Preserve ordinary Node/Electron configuration; disable only a verified local opt-in. */
export function libJSUtilityAuthForwarding(value: boolean | undefined): boolean | undefined {
  if (typeof host.versions.node === 'string' || typeof host.versions.electron === 'string') {
    return value;
  }
  const local = host._vscodeLibJSLocalDesktopStatus;
  const engine = host._vscodeLibJSEngineStatus;
  if (
    typeof host.versions.mite !== 'string' ||
    typeof host.versions.libjs !== 'string' ||
    host.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP !== '1' ||
    host.env.VSCODE_CODE_CACHE_PATH ||
    local?.enabled !== true ||
    local.scope !== 'experimental-local-desktop' ||
    !Array.isArray(local.disabled) ||
    !local.disabled.includes('engine-code-cache') ||
    engine?.engine !== 'LibJS/LibWeb' ||
    engine.scope !== 'implicit-code-oss-engine-tuning-only' ||
    (value !== undefined && typeof value !== 'boolean')
  ) {
    throw Object.assign(
      new Error('Code-OSS utility authentication opt-out requires verified local LibJS policy'),
      {
        code: 'ERR_CODE_OSS_LIBJS_UTILITY_AUTH_UNAVAILABLE'
      }
    );
  }
  if (value !== true) {
    return value;
  }
  if (!published) {
    if (Object.hasOwn(host, '_vscodeLibJSUtilityAuthStatus')) {
      throw Object.assign(new Error('Conflicting Code-OSS utility authentication status'), {
        code: 'ERR_CODE_OSS_LIBJS_UTILITY_AUTH_UNAVAILABLE'
      });
    }
    published = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-local-utility-auth-opt-out-only',
      available: false,
      requested: true,
      reason:
        'Utility HTTP authentication forwarding is unavailable; the local app disables this optional integration'
    });
    Object.defineProperty(host, '_vscodeLibJSUtilityAuthStatus', {
      value: published,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return false;
}

/** Opt out of the pinned app request; never advertise unsigned native-addon support. */
export function libJSUtilityUnsignedLibraries(value: boolean | undefined): boolean | undefined {
  if (typeof host.versions.node === 'string' || typeof host.versions.electron === 'string') {
    return value;
  }
  const local = host._vscodeLibJSLocalDesktopStatus;
  const engine = host._vscodeLibJSEngineStatus;
  if (
    typeof host.versions.mite !== 'string' ||
    typeof host.versions.libjs !== 'string' ||
    host.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP !== '1' ||
    host.env.VSCODE_CODE_CACHE_PATH ||
    local?.enabled !== true ||
    local.scope !== 'experimental-local-desktop' ||
    !Array.isArray(local.disabled) ||
    !local.disabled.includes('engine-code-cache') ||
    engine?.engine !== 'LibJS/LibWeb' ||
    engine.scope !== 'implicit-code-oss-engine-tuning-only' ||
    (value !== undefined && typeof value !== 'boolean')
  ) {
    throw Object.assign(
      new Error('Code-OSS utility unsigned-library opt-out requires verified local LibJS policy'),
      {
        code: 'ERR_CODE_OSS_LIBJS_UTILITY_UNSIGNED_LIBRARIES_UNAVAILABLE'
      }
    );
  }
  if (value !== true) {
    return value;
  }
  if (!unsignedPublished) {
    if (Object.hasOwn(host, '_vscodeLibJSUtilityUnsignedLibrariesStatus')) {
      throw Object.assign(new Error('Conflicting Code-OSS utility unsigned-library status'), {
        code: 'ERR_CODE_OSS_LIBJS_UTILITY_UNSIGNED_LIBRARIES_UNAVAILABLE'
      });
    }
    unsignedPublished = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-local-utility-unsigned-library-opt-out-only',
      available: false,
      requested: true,
      reason:
        'Unsigned-library loading relaxation is unavailable; this opt-out neither loads NAPI addons nor changes macOS entitlements'
    });
    Object.defineProperty(host, '_vscodeLibJSUtilityUnsignedLibrariesStatus', {
      value: unsignedPublished,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return false;
}
