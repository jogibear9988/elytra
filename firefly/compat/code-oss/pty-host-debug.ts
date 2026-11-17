/** Disable only Code-OSS's implicit development inspector in the explicit local LibJS profile. */
type Args = { 'inspect-ptyhost'?: unknown; 'inspect-brk-ptyhost'?: unknown };
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
  _vscodeLibJSPtyHostDebugStatus?: Status;
};
const host = process as typeof process & Host;
let published: Status | undefined;
function refuse(): never {
  throw Object.assign(new Error('Code-OSS cannot apply pty-host inspector options on LibJS'), {
    code: 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
  });
}

/** Ordinary Node/Electron invokes the unchanged upstream parser exactly once. */
export function libJSPtyHostDebugParams<A extends Args, T extends DebugParams>(
  args: A,
  isBuilt: boolean,
  parse: (args: A, isBuilt: boolean) => T
): T {
  if (typeof host.versions.node === 'string' || typeof host.versions.electron === 'string') {
    return parse(args, isBuilt);
  }
  function admit(): void {
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
      args['inspect-ptyhost'] !== undefined ||
      args['inspect-brk-ptyhost'] !== undefined
    ) {
      refuse();
    }
  }
  admit();
  const result = parse(args, isBuilt);
  admit();
  // The pinned parser returns ordinary own data fields. Reject malformed or
  // accessor-backed replacements rather than executing hooks while publishing
  // the unavailable-capability record. Parser exceptions propagate unchanged.
  if (!result || typeof result !== 'object') refuse();
  const descriptors = Object.getOwnPropertyDescriptors(result);
  if (
    Reflect.ownKeys(descriptors).some(
      (key) => !Object.hasOwn(Reflect.get(descriptors, key), 'value')
    )
  )
    refuse();
  const port = descriptors.port?.value;
  const shouldBreak = descriptors.break?.value;
  admit();
  // Built applications with no debugging keep the original result and identity.
  if (isBuilt && port === null && shouldBreak === false) return result;
  if (isBuilt || port !== 5877 || shouldBreak !== false) refuse();
  if (!published) {
    if (Object.hasOwn(host, '_vscodeLibJSPtyHostDebugStatus')) refuse();
    published = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-local-implicit-pty-host-inspector-only',
      feature: 'implicit-pty-host-inspector',
      available: false,
      omitted: Object.freeze(['--nolazy', '--inspect=5877']),
      reason:
        'The implicit Code-OSS development inspector is unavailable on LibJS; explicit debugging requests remain errors'
    });
    Object.defineProperty(host, '_vscodeLibJSPtyHostDebugStatus', {
      value: published,
      enumerable: false,
      writable: false,
      configurable: false
    });
  }
  return { ...result, port: null };
}
