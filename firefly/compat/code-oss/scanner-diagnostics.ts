/** Opt-in CA-29 development observation, removed with the scanner stall fix.
 * No paths, manifest data, environment, URLs, timers or scheduling changes.
 * Missing "after" means the original statement has not successfully completed;
 * it does not distinguish an outstanding await from an original thrown error.
 */
const phases = new Set([
  'input-translations',
  'input-mtime',
  'input-profile-mtime',
  'stat',
  'directory',
  'manifest',
  'translate',
  'nls-translation',
  'nls-exists',
  'nls-find',
  'nls-localized',
  'nls-original',
  'profile-scan',
  'cache-read',
  'cache-write'
]);
let sequence = 0;
let remaining = 1024;

function write(phase: string, counter: number, edge: 'before' | 'after'): void {
  if (
    !remaining ||
    !phases.has(phase) ||
    !Number.isSafeInteger(counter) ||
    counter <= 0 ||
    counter > sequence
  ) {
    return;
  }
  --remaining;
  try {
    console.error(`[ELYTRA_EXTENSION_SCAN] phase=${phase} edge=${edge} counter=${counter}`);
  } catch {
    // Diagnostics must never replace the original scanner outcome.
  }
}

export function scanBegin(phase: string): number {
  if (!remaining || !phases.has(phase)) {
    return 0;
  }
  const counter = ++sequence;
  write(phase, counter, 'before');
  return counter;
}

export function scanEnd(phase: string, counter: number): void {
  write(phase, counter, 'after');
}
