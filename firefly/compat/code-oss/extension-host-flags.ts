/** Local application defaults only; no V8 inspector, DNS-order or network-inspection implementation. */
import { isLocalDesktop, localDesktopStatus } from './elytraLocalDesktop.js';
type Options = {
  args: Record<string, unknown>;
  isBuilt: boolean;
  debug: { port: number | null; break: boolean; debugId?: unknown; env?: unknown };
  development: boolean;
  tests: unknown;
};
type Host = {
  versions?: Record<string, string | undefined>;
  env?: Record<string, string | undefined>;
  _vscodeLibJSEngineStatus?: { engine?: string; scope?: string };
};
declare const process: Host;
const globals = globalThis as typeof globalThis & {
  vscode?: { process?: Host };
  _vscodeLibJSExtensionHostFlagsStatus?: unknown;
};
const host = typeof process !== 'undefined' ? process : globals.vscode?.process;
let published: unknown;
function refuse(): never {
  throw Object.assign(
    new Error('Local LibJS extension hosts cannot apply requested debugging or engine options'),
    {
      code: 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
    }
  );
}
function validate(read: () => Options): Options {
  const versions = host?.versions;
  const local = localDesktopStatus();
  if (
    !isLocalDesktop() ||
    local.enabled !== true ||
    local.scope !== 'experimental-local-desktop' ||
    !local.disabled.includes('engine-code-cache') ||
    typeof versions?.libjs !== 'string' ||
    (typeof versions.mite !== 'string' && typeof versions.firefly !== 'string') ||
    host?.env?.VSCODE_CODE_CACHE_PATH
  )
    refuse();
  // The sandbox has genuine firefly/libjs identities, not a fabricated Mite
  // version or a main-process engine-status property on its frozen process copy.
  if (
    typeof versions.mite === 'string' &&
    (host?._vscodeLibJSEngineStatus?.engine !== 'LibJS/LibWeb' ||
      host?._vscodeLibJSEngineStatus?.scope !== 'implicit-code-oss-engine-tuning-only')
  )
    refuse();
  const options = read();
  if (
    !options ||
    typeof options.isBuilt !== 'boolean' ||
    options.development !== false ||
    options.tests !== undefined ||
    !options.args ||
    typeof options.args !== 'object' ||
    !options.debug ||
    options.debug.break !== false ||
    options.debug.debugId !== undefined ||
    options.debug.env !== undefined ||
    options.debug.port !== (options.isBuilt ? null : 5870)
  )
    refuse();
  for (const key of [
    'inspect-extensions',
    'inspect-brk-extensions',
    'extensionDevelopmentPath',
    'extensionTestsPath',
    'js-flags'
  ]) {
    if (options.args[key] !== undefined) refuse();
  }
  // The upstream minimist parser includes absent boolean options as false.
  // Raw CLI presence is rejected by the owned main startup engine policy.
  if (
    options.args['prof-v8-extensions'] !== undefined &&
    options.args['prof-v8-extensions'] !== false
  )
    refuse();
  return options;
}
/** Reject explicit debug/test/profiling before port probing, environment or process mutations. */
export function libJSExtensionHostPolicy(
  read: () => Options
): ((flags: string[], port: number) => string[]) | undefined {
  if (typeof host?.versions?.node === 'string' || typeof host?.versions?.electron === 'string') {
    return undefined;
  }
  validate(read);
  return (flags, port) => {
    const options = validate(read);
    if (!Number.isInteger(port) || port < 0 || port > 65535 || (options.isBuilt && port !== 0))
      refuse();
    const expected = [
      '--dns-result-order=ipv4first',
      '--experimental-network-inspection',
      ...(port ? ['--nolazy', `--inspect=127.0.0.1:${port}`] : ['--inspect-port=0'])
    ];
    if (
      !Array.isArray(flags) ||
      flags.length !== expected.length ||
      flags.some((value, index) => value !== expected[index])
    )
      refuse();
    if (!published) {
      if (Object.hasOwn(globals, '_vscodeLibJSExtensionHostFlagsStatus')) refuse();
      published = Object.freeze({
        engine: 'LibJS/LibWeb',
        scope: 'code-oss-local-implicit-extension-host-options-only',
        inspectorAvailable: false,
        networkInspectionAvailable: false,
        dnsOrderingApplied: false,
        omitted: Object.freeze([
          'implicit-inspector-configuration',
          '--dns-result-order=ipv4first',
          '--experimental-network-inspection'
        ]),
        reason:
          'Implicit development/runtime defaults are omitted; no inspector, network inspection or requested IPv4-first ordering is provided'
      });
      Object.defineProperty(globals, '_vscodeLibJSExtensionHostFlagsStatus', {
        value: published,
        writable: false,
        configurable: false,
        enumerable: false
      });
    }
    return [];
  };
}
