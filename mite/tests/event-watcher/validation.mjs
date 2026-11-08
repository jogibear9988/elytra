/** Record actual argument/native admission errors; explicit exit bounds Node's failed-argument handles. */
import fs from 'node:fs';
fs.writeFileSync('validation.txt', 'baseline');
const rows = [];

function observe(callback) {
  try {
    const watcher = callback();
    watcher?.close();
    rows.push('ACCEPTED');
  } catch (error) {
    rows.push([error.name, error.code ?? null]);
  }
}

for (const value of [undefined, 42, 'bad\0path', 'missing-watch-target']) {
  observe(() => fs.watch(value));
}
for (const options of [
  null,
  [],
  42,
  { encoding: 'not-an-encoding' },
  { encoding: null },
  { encoding: 0 },
  { persistent: 0 },
  { persistent: 'false' },
  { recursive: 0 },
  { signal: null },
  { signal: {} }
]) {
  observe(() => fs.watch('validation.txt', options));
}

observe(() => fs.watch('validation.txt', {}, false));

observe(() => fs.watch('validation.txt', {}, true));

console.log(JSON.stringify(rows));
// Node may create native watchers before rejecting listener/signal validation.
// Only this test-owned process exits; no independent child process is created.
process.exit(0);
