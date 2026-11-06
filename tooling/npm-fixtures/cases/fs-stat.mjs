import stats from '@nodelib/fs.stat';
import path from 'node:path';
const root = process.argv[2];

function inspect(name, options) {
  return new Promise((resolve) =>
    stats.stat(path.join(root, name), options, (error, s) => {
      resolve(
        error
          ? [error.code, error.syscall]
          : [s.isFile(), s.isDirectory(), s.isSymbolicLink(), typeof s.size]
      );
    })
  );
}

const pending = inspect('plain.txt');
console.log('scheduled');
console.log(JSON.stringify(await pending));
for (const [name, options] of [
  ['folder', {}],
  ['link', {}],
  ['link', { followSymbolicLink: false }],
  ['link', { markSymbolicLink: true }],
  ['broken', {}],
  ['broken', { throwErrorOnBrokenSymbolicLink: false }],
  ['missing', {}]
]) {
  console.log(JSON.stringify(await inspect(name, options)));
}
const settings = new stats.Settings();
await new Promise((resolve, reject) =>
  stats.stat(path.join(root, 'plain.txt'), settings, (error, s) => {
    if (error) {
      reject(error);
      return;
    }
    console.log(s.size, s.isFile());
    resolve();
  })
);
