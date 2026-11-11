/** Disable only Code-OSS's implicit development inspector in the explicit local LibJS profile. */
type Args = { 'inspect-sharedprocess'?: unknown; 'inspect-brk-sharedprocess'?: unknown };
type DebugParams = { port: number | null; break: boolean };
type Status = {
  engine: string;
  scope: string;
  feature: string;
  available: false;
  omitted: readonly string[];
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
  _vscodeLibJSSharedProcessDebugStatus?: Status;
};
const host = process as typeof process & Host;
let published: Status | undefined;
function refuse(): never {
  throw Object.assign(
    new Error('Code-OSS cannot apply shared-process inspector options on LibJS'),
    {
      code: 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
    }
  );
}

/** Ordinary Node/Electron invokes the unchanged upstream parser exactly once. */
export function libJSSharedProcessDebugParams<A extends Args, T extends DebugParams>(
  args: A,
  isBuilt: boolean,
  parse: (args: A, isBuilt: boolean) => T
): T {
  if (typeof host.versions.node === 'string' || typeof host.versions.electron === 'string') {
    return parse(args, isBuilt);
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
    typeof isBuilt !== 'boolean' ||
    !args ||
    typeof args !== 'object' ||
    args['inspect-sharedprocess'] !== undefined ||
    args['inspect-brk-sharedprocess'] !== undefined
  ) {
    refuse();
  }
  const result = parse(args, isBuilt);
  // Built applications with no debugging keep the original result and identity.
  if (isBuilt && result.port === null && result.break === false) return result;
  if (isBuilt || result.port !== 5879 || result.break !== false) refuse();
  if (!published) {
    if (Object.hasOwn(host, '_vscodeLibJSSharedProcessDebugStatus')) refuse();
    published = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-local-implicit-shared-process-inspector-only',
      feature: 'implicit-shared-process-inspector',
      available: false,
      omitted: Object.freeze(['--nolazy', '--experimental-network-inspection', '--inspect=5879']),
      reason:
        'The implicit Code-OSS development inspector is unavailable on LibJS; explicit debugging requests remain errors'
    });
    Object.defineProperty(host, '_vscodeLibJSSharedProcessDebugStatus', {
      value: published,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return { ...result, port: null };
}
