/** External runner proves the watcher alone keeps the process alive until a real change. */
import fs from 'node:fs';
const mode = process.argv[2];
fs.writeFileSync('reference.txt', 'baseline');

const primary = () => {};

const changed = () => {
  fs.unwatchFile('reference.txt');
  console.log('REF_CHANGED');
};

let watcher;
if (mode === 'explicit') {
  watcher = fs.watchFile('reference.txt', { interval: 20, persistent: false }, changed);
  watcher.ref();
} else if (mode === 'shared') {
  fs.watchFile('reference.txt', { interval: 20 }, primary);
  watcher = fs.watchFile('reference.txt', { persistent: false, interval: 5007 }, changed);
  fs.unwatchFile('reference.txt', primary);
} else {
  watcher = fs.watchFile('reference.txt', { interval: 20, persistent: 'false' }, changed);
}
const deadline = setTimeout(() => {
  throw Error('Referenced watcher never observed external change');
}, 4000);
deadline.unref();
fs.writeFileSync('reference-ready', 'ready');
console.log('REF_READY');
