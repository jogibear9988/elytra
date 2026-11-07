/**
 * Application-only policy for Code-OSS's optional Node/V8 cache. This module is
 * explicitly selected by the audited source overlay, never registered as a Mite
 * node:module or node:v8 implementation. No bytecode or V8 statistics are invented.
 */
export const constants = Object.freeze({
  compileCacheStatus: Object.freeze({ FAILED: 0, ENABLED: 1, ALREADY_ENABLED: 2, DISABLED: 3 })
});

function unsupported(code: string, message: string): never {
  const error = new Error(message) as Error & { code: string };
  error.code = code;
  throw error;
}

/** Refuse generation/measurement modes before the upstream code touches disk. */
export function assertLibJSCacheMode(operation: 'enable' | 'ready' | 'wait'): void {
  if (process.env['VSCODE_GENERATE_NODE_COMPILE_CACHE'] === '1') {
    unsupported(
      'ERR_CODE_OSS_LIBJS_CACHE_GENERATION',
      'LibJS cannot generate a Node/V8 compile cache'
    );
  }
  if (process.env['VSCODE_NODE_COMPILE_CACHE_MEASUREMENTS']) {
    unsupported(
      'ERR_CODE_OSS_LIBJS_CACHE_MEASUREMENTS',
      'V8 cache measurements are unavailable on LibJS'
    );
  }
  if (operation === 'wait') {
    unsupported(
      'ERR_CODE_OSS_LIBJS_CACHE_WAIT',
      'No Node/V8 cache readiness markers exist on LibJS'
    );
  }
}

/** Keep the upstream disabled-result branch, including its ordinary warning. */
export function enableCompileCache(_options?: unknown): {
  status: number;
  message: string;
  directory?: string;
} {
  assertLibJSCacheMode('enable');
  return {
    status: constants.compileCacheStatus.DISABLED,
    message: 'Node/V8 compile caches are disabled on LibJS'
  };
}

export function getCompileCacheDir(): undefined {
  return undefined;
}

export function flushCompileCache(): never {
  return unsupported(
    'ERR_CODE_OSS_LIBJS_CACHE_GENERATION',
    'No Node/V8 compile cache is available to flush'
  );
}

export function getHeapCodeStatistics(): never {
  return unsupported(
    'ERR_CODE_OSS_LIBJS_V8_STATISTICS',
    'V8 heap code statistics do not describe LibJS'
  );
}
