/** Unreferenced real native watch must not keep the runtime alive. */
import fs from 'node:fs';
fs.writeFileSync('unref.txt', 'baseline');
const watcher = fs.watch('unref.txt', { persistent: false }, () => {
  throw Error('Unexpected unchanged event');
});
if (process.argv[2] === 'explicit') {
  watcher.ref().unref();
}
console.log('UNREF_DONE');
