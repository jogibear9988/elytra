/** Genuine uv_fs_poll callback/handle lifetime across explicit embedded LibJS collection. */
import fs from 'node:fs';
if (
  typeof globalThis.collectHostGarbage !== 'function' ||
  typeof globalThis.collectHostGarbageAtIdle !== 'function'
) {
  throw Error('Require actual embedded collector host');
}

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const turn = () => new Promise((resolve) => setTimeout(resolve, 10));

const file = 'gc-poll.txt';
fs.writeFileSync(file, 'baseline');
const deadline = setTimeout(() => {
  throw Error('Native stat watcher GC probe timed out');
}, 4000);

function install() {
  const marker = { alive: 'owned callback payload' };
  let finish;
  const completed = new Promise((resolve) => (finish = resolve));

  const listener = (curr, prev) => {
    check(
      marker.alive === 'owned callback payload' && curr.size === 14 && prev.size === 8,
      'Actual native event retains callback closure and stats'
    );
    fs.unwatchFile(file, listener);
    // The native callback is currently executing. Closing and collecting here
    // must preserve its arguments/frame until the native dispatch has unwound.
    collectHostGarbage();
    check(curr.isFile() && prev.isFile(), 'Stats remain alive during reentrant unwatch/GC');
    finish();
  };

  const watcher = fs.watchFile(file, { interval: 20 }, listener);
  return { callback: new WeakRef(listener), watcher: new WeakRef(watcher), completed };
}

const installed = install();
// End the WeakRef constructor keep-alive job before the first native collection.
await turn();
collectHostGarbage();

function checkActive(phase) {
  check(
    typeof phase.callback.deref() === 'function' && phase.watcher.deref() !== undefined,
    'Registered callback/watcher rooted without application references'
  );
}

checkActive(installed);
await turn();
fs.writeFileSync(file, 'native-changed');
await installed.completed;
// Do not dereference between collections: an await frame may retain the result
// of the preceding WeakRef inspection until that frame resumes. Observe once,
// after independent collection turns, as in the existing native I/O GC probes.
for (let attempt = 0; attempt < 12; attempt++) {
  await turn();
  // Collect after native dispatch frames unwind: Windows conservatively scans
  // those stacks, where an expired callback slot is not application ownership.
  await collectHostGarbageAtIdle();
}
check(
  installed.callback.deref() === undefined && installed.watcher.deref() === undefined,
  'Unwatch/uv_close releases callback and watcher roots'
);
clearTimeout(deadline);
console.log(
  'PASS actual native stat watcher active roots, callback unwatch/GC reentry and final reclamation'
);
