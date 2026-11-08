/** Real initial missing-file sentinel and BigIntStats transition to existence. */
import fs from 'node:fs';
import path from 'node:path';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const file = path.resolve('initially-missing.txt');
const rows = [];
const watcher = fs.watchFile(
  file,
  { interval: 20, persistent: false, bigint: true },
  (curr, prev) => rows.push({ curr, prev })
);
const deadline = setTimeout(() => {
  throw Error('Missing polling sentinel timed out');
}, 3000);
while (!rows.length) {
  await new Promise((resolve) => setTimeout(resolve, 5));
}
const absent = rows[0];
check(
  absent.curr.constructor.name === 'BigIntStats' && typeof absent.curr.ino === 'bigint',
  'Actual BigIntStats'
);
for (const stats of [absent.curr, absent.prev]) {
  for (const name of ['dev', 'ino', 'nlink', 'size', 'ctimeMs', 'mtimeNs']) {
    check(stats[name] === 0n, 'Initial missing stats zero ' + name);
  }
  check(stats.ctime.getTime() === 0 && stats.mtime.getTime() === 0, 'BigInt sentinel date epochs');
}
await new Promise((resolve) => setTimeout(resolve, 80));
check(rows.length === 1, 'Repeated unchanged ENOENT does not repeat callback');
fs.writeFileSync(file, 'created');
while (rows.length < 2) {
  await new Promise((resolve) => setTimeout(resolve, 5));
}
check(
  rows[1].curr.size === 7n && rows[1].curr.isFile() && rows[1].prev.ino === 0n,
  'Missing -> present uses real current and zero previous'
);
fs.unwatchFile(file);
clearTimeout(deadline);
check(watcher.listenerCount('change') === 0, 'Unwatch drops bigint listener');
console.log(
  JSON.stringify({ missingZeroPair: true, bigint: true, repeatedMissing: false, created: true })
);
