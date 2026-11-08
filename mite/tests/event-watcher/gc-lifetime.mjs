/** Actual native events retain callbacks until close; explicit collector runs inside dispatch. */
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

fs.writeFileSync('gc-event.txt', 'baseline');
let calls = 0;
const deadline = setTimeout(() => {
  throw Error('Native event watcher GC probe timed out');
}, 4000);

function install() {
  const marker = { payload: 'retained native callback' };
  let finish;
  const completed = new Promise((resolve) => (finish = resolve));

  const listener = (kind, name) => {
    calls++;
    check(
      marker.payload === 'retained native callback' && (kind === 'change' || kind === 'rename'),
      'Native callback closure survives actual GC'
    );
    check(name === null || name === 'gc-event.txt', 'Native filename snapshot survives dispatch');
    watcher.close();
    collectHostGarbage();
    check(
      marker.payload === 'retained native callback',
      'Close/GC inside actual native event preserves active frame'
    );
    finish();
  };

  const watcher = fs.watch('gc-event.txt', listener);
  return { callback: new WeakRef(listener), watcher: new WeakRef(watcher), completed };
}

function inspectActive(phase) {
  check(
    typeof phase.callback.deref() === 'function' && phase.watcher.deref() !== undefined,
    'Actual native watcher owns registered callbacks without application references'
  );
}

const installed = install();
await turn();
collectHostGarbage();
// Inspection uses a completed helper frame so no dereferenced owner survives in
// the module's long-lived await registers during the later reclamation test.
inspectActive(installed);
await turn();
fs.appendFileSync('gc-event.txt', '-changed');
await installed.completed;
for (let attempt = 0; attempt < 12; attempt++) {
  await turn();
  await collectHostGarbageAtIdle();
}
check(
  calls === 1 &&
    installed.callback.deref() === undefined &&
    installed.watcher.deref() === undefined,
  'Close retires native callback and wrapper roots after dispatch'
);
clearTimeout(deadline);
console.log(
  'PASS actual native event watcher active roots, close/GC callback reentry and final reclamation'
);
