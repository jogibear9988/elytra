/** Unreferenced polling survives registration but does not keep the runtime alive. */
import fs from 'node:fs';
fs.writeFileSync('unref.txt', 'existing');
const watcher = fs.watchFile('unref.txt', { interval: 20, persistent: false }, () => {
  throw Error('Unchanged file emitted');
});
if (process.argv[2] === 'explicit') {
  watcher.ref().unref();
}
console.log('UNREF_DONE');
