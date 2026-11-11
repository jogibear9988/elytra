/**
 * Explicit Code-OSS application adaptation for LibJS/LibWeb. This is not an
 * Electron command-line shim: user-requested unsupported engine policies fail.
 * The ordinary Node/Electron branch keeps upstream tuning unchanged.
 */
type Options = Record<string, unknown>;
type CommandLine = { hasSwitch(name: string): boolean };

export function isLibJSHost(): boolean {
  return (
    typeof process.versions.mite === 'string' &&
    process.versions.node === undefined &&
    process.versions.electron === undefined
  );
}

const unsupportedSwitches = Object.freeze([
  'enable-features',
  'disable-features',
  'enable-blink-features',
  'disable-blink-features',
  'js-flags',
  'inspect-sharedprocess',
  'inspect-brk-sharedprocess',
  'xdg-portal-required-version',
  'max-active-webgl-contexts',
  'disable-hardware-acceleration',
  'disable-gpu',
  'disable-lcd-text',
  'force-color-profile',
  'proxy-bypass-list',
  'proxy-server',
  'remote-debugging-port',
  'force-renderer-accessibility',
  'password-store',
  'enable-rdp-display-tracking',
  'no-sandbox',
  'disable-gpu-sandbox',
  'disable-chromium-sandbox'
]);

function refuse(name: string, source: string): never {
  const error = new Error(`Code-OSS on LibJS cannot apply ${name} from ${source}`) as Error & {
    code: string;
  };
  error.code = 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION';
  throw error;
}

/** Run before upstream argv.json processing can mutate app options/process.argv. */
export function assertLibJSStartupOptions(
  cli: Options,
  config: Options,
  commandLine: CommandLine
): void {
  if (!isLibJSHost()) {
    return;
  }
  if (cli['sandbox'] !== true) {
    refuse('sandbox disable/ambiguous sandbox policy', 'command line');
  }
  for (const name of unsupportedSwitches) {
    // Native presence also catches empty/false-valued flags that a CLI parser
    // could otherwise normalize away. Values never enter status metadata.
    if (commandLine.hasSwitch(name)) {
      refuse(name, 'native command line');
    }
    for (const [source, options] of [
      ['command line', cli],
      ['argv.json', config]
    ] as const) {
      // minimist mirrors the default sandbox:true into its no-sandbox alias.
      // Inspect canonical cli.sandbox above and raw native presence instead.
      if (source === 'command line' && name === 'no-sandbox') {
        continue;
      }
      const value = options[name];
      if (value !== undefined && value !== false && value !== '') {
        refuse(name, source);
      }
    }
  }
}

/** Audit only implicit upstream defaults; these are not implemented capabilities. */
export function recordLibJSDefaults(): void {
  if (!isLibJSHost()) {
    throw new Error('LibJS engine adaptation requires the actual Mite host');
  }
  const omitted = [
    {
      name: 'NetAdapterMaxBufSizeFeature',
      reason: 'Chromium NetToMojo buffer tuning does not configure Mite networking'
    },
    {
      name: 'DocumentPolicyIncludeJSCallStacksInCrashReports',
      reason:
        'Chromium JavaScript stack collection is unavailable; native Firefly reports are separate'
    },
    {
      name: 'EarlyEstablishGpuChannel',
      reason: 'Chromium GPU-channel startup tuning is not a LibWeb setting'
    },
    {
      name: 'EstablishGpuChannelAsync',
      reason: 'Chromium GPU-channel startup tuning is not a LibWeb setting'
    },
    {
      name: 'UpdateCompositorMemoryPolicyOnScreenChange',
      reason: 'Chromium compositor budgets do not configure the Ladybird compositor'
    },
    {
      name: 'CalculateNativeWinOcclusion',
      reason: 'Chromium native window occlusion tracking is not used'
    },
    {
      name: 'FontMatchingCTMigration',
      reason: 'Blink font-matching migration flags do not configure LibWeb fonts'
    },
    {
      name: 'StandardizedBrowserZoom',
      reason: 'Blink layout/zoom feature switches are not implemented'
    },
    {
      name: 'xdg-portal-required-version',
      reason: 'Electron portal-version selection is not provided by Firefly'
    },
    {
      name: 'max-active-webgl-contexts',
      reason: 'Chromium WebGL context quotas do not configure LibWeb'
    }
  ];
  if (process.platform === 'linux') {
    omitted.push({
      name: 'GlobalShortcutsPortal',
      reason: 'Electron Wayland global shortcuts are not implemented by Firefly'
    });
  }
  const status = Object.freeze({
    engine: 'LibJS/LibWeb',
    scope: 'implicit-code-oss-engine-tuning-only',
    rendererCompatibility: 'not-established',
    sandbox: 'upstream-enable-path-required',
    omitted: Object.freeze(omitted.map((entry) => Object.freeze(entry)))
  });
  Object.defineProperty(process, '_vscodeLibJSEngineStatus', {
    value: status,
    enumerable: false,
    writable: false,
    configurable: false
  });
}
