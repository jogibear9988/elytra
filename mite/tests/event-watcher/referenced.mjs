/** External runner changes the file only after proving the native watcher alone retains the process. */
import fs from 'node:fs';
const mode = process.argv[2];
fs.writeFileSync('reference.txt', 'baseline');
let watcher;
watcher = fs.watch('reference.txt', { persistent: mode !== 'explicit' }, () => {
  watcher.close();
  console.log('REF_CHANGED');
});
if (mode === 'explicit') {
  watcher.ref();
}
const deadline = setTimeout(() => {
  throw Error('Referenced native watcher never observed external change');
}, 4000);
deadline.unref();
fs.writeFileSync('reference-ready', 'ready');
console.log('REF_READY');
