/** Explicit local-app deferral of the agent's optional Node worker-backed diff service. */
import type { Worker } from 'worker_threads';
import { isLocalDesktop, localDesktopStatus } from './elytraLocalDesktop.js';
import { assertLibJSCacheMode } from '../node/elytraCompileCache.js';

type Status = {
  engine: 'LibJS/LibWeb';
  scope: 'code-oss-local-agent-diff-worker-only';
  feature: 'agent-diff-worker';
  available: false;
  reason: string;
};
const host = process as typeof process & {
  _vscodeLibJSAgentDiffWorkerStatus?: Status;
};
const versions = host.versions as Record<string, string | undefined>;
// On Node/Electron, resolve the actual constructor before the adapted service
// evaluates. Mite never executes this import, and no Worker class is fabricated.
const nodeWorker =
  typeof versions.node === 'string' || typeof versions.electron === 'string'
    ? (await import('worker_threads')).Worker
    : undefined;
let published: Status | undefined;

function unavailable(message: string): never {
  throw Object.assign(new Error(message), {
    code: 'ERR_CODE_OSS_LIBJS_DIFF_WORKER_UNAVAILABLE'
  });
}

/** Called only at the original worker-construction site, never during service setup. */
export function requireAgentDiffWorker(): typeof Worker {
  if (nodeWorker) return nodeWorker;
  const local = localDesktopStatus();
  if (
    typeof versions.mite !== 'string' ||
    typeof versions.libjs !== 'string' ||
    versions.node !== undefined ||
    versions.electron !== undefined ||
    host.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP !== '1' ||
    host.env.VSCODE_CODE_CACHE_PATH ||
    !isLocalDesktop() ||
    !local.enabled ||
    local.scope !== 'experimental-local-desktop' ||
    !local.disabled.includes('engine-code-cache')
  ) {
    return unavailable('Agent diff workers require the verified local LibJS profile');
  }
  assertLibJSCacheMode('enable');
  if (!published) {
    if (Object.hasOwn(host, '_vscodeLibJSAgentDiffWorkerStatus')) {
      return unavailable('Conflicting agent diff worker status');
    }
    const status: Status = Object.freeze({
      engine: 'LibJS/LibWeb',
      scope: 'code-oss-local-agent-diff-worker-only',
      feature: 'agent-diff-worker',
      available: false,
      reason:
        'Mite has no worker_threads backend; agent worker-backed diff requests are unavailable'
    });
    Object.defineProperty(host, '_vscodeLibJSAgentDiffWorkerStatus', {
      value: status,
      enumerable: false,
      configurable: false,
      writable: false
    });
    published = status;
  }
  return unavailable('Agent worker-backed diff computation is unavailable on LibJS');
}
