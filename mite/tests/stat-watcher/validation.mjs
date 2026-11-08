/** Node-compatible path/options validation, duplicate registration and remove-one behavior. */
import fs from 'node:fs';
import path from 'node:path';
const rows = [];
const file = path.resolve('validation.txt');
fs.writeFileSync(file, 'existing');

function observe(callback) {
  try {
    const watcher = callback();
    watcher?.unref();
    rows.push('ACCEPTED');
  } catch (error) {
    rows.push([error.name, error.code ?? null]);
  }
}

for (const value of [Buffer.from(file), undefined, 42, file + '\0bad']) {
  observe(() => fs.watchFile(value, { interval: 20, persistent: false }, () => {}));

  observe(() => fs.unwatchFile(value));
}
for (const interval of [null, '20', -1, 0.5, 4294967296, 0, 4294967295]) {
  observe(() => fs.watchFile(file, { interval, persistent: false }, () => {}));

  fs.unwatchFile(file);
}

observe(() => fs.watchFile(file, { interval: 20, persistent: false }, undefined));

const callback = () => {};

const one = fs.watchFile(file, { interval: 20, persistent: false }, callback);
const two = fs.watchFile(file, { interval: 5007, persistent: false }, callback);

const ignored = () => {};

const three = fs.watchFile(file, { interval: -1, bigint: true, persistent: 'false' }, ignored);
if (three !== one) {
  throw Error('Existing watcher unexpectedly reconfigured');
}
fs.unwatchFile(file, ignored);
if (one !== two || one.listenerCount('change') !== 2) {
  throw Error('Duplicate listener registrations lost');
}
fs.unwatchFile(file, callback);
if (one.listenerCount('change') !== 1) {
  throw Error('Unwatch must remove one duplicate listener');
}
fs.unwatchFile(file, 123);
if (one.listenerCount('change') !== 0) {
  throw Error('Nonfunction listener removes all callbacks');
}
console.log(JSON.stringify(rows));
