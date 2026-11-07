/** Real raw filesystem operations; also runs in Electron's console-only Node mode. */
import raw, { readFileSync, promises } from 'original-fs';
import fs from 'node:fs';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

const require = createRequire(import.meta.url);
const file = process.argv[2];
const url = pathToFileURL(file);
check(raw !== fs && promises !== fs.promises, 'separate raw/public facades');
check(
  require('original-fs') === raw && require.resolve('original-fs') === 'original-fs',
  'module identity'
);
check(readFileSync === raw.readFileSync && promises === raw.promises, 'named export identity');
const original = readFileSync(url);
check(original.length === 4096, 'raw native bytes');
check(
  raw.readFileSync(file + '.asar').equals(original),
  'raw .asar bytes without virtual archive lookup'
);
check(raw.statSync(file).isFile(), 'physical file stat');
check((await promises.readFile(url)).equals(original), 'raw Promise read');
await new Promise((resolve, reject) =>
  raw.readFile(file, (error, bytes) => {
    if (error) {
      return reject(error);
    }
    try {
      check(bytes.equals(original), 'raw callback bytes');
      resolve();
    } catch (error) {
      reject(error);
    }
  })
);

// Native descriptors cross the two facades in both directions.
for (const [opener, reader, closer] of [
  [raw, fs, raw],
  [fs, raw, fs]
]) {
  const fd = opener.openSync(file, 'r');
  try {
    const bytes = Buffer.alloc(32);
    check(reader.readSync(fd, bytes, 0, 32, 7) === 32, 'cross-facade descriptor read');
    check(bytes.equals(original.subarray(7, 39)), 'cross-facade exact bytes');
  } finally {
    closer.closeSync(fd);
  }
}
const handle = await promises.open(file, 'r');
try {
  const bytes = Buffer.alloc(16);
  check(
    fs.readSync(handle.fd, bytes, 0, 16, 0) === 16 && bytes.equals(original.subarray(0, 16)),
    'raw FileHandle fd'
  );
} finally {
  await handle.close();
}

// Replacing ordinary fs methods must not poison raw facade methods or their closures.
const saved = {
  readFileSync: fs.readFileSync,
  writeFileSync: fs.writeFileSync,
  accessSync: fs.accessSync,
  readFile: fs.promises.readFile
};

const poison = () => {
  throw new Error('PUBLIC_FS_PATCH_LEAK');
};

try {
  fs.readFileSync = poison;
  fs.writeFileSync = poison;
  fs.accessSync = poison;
  fs.promises.readFile = poison;
  check(raw.readFileSync(file).equals(original), 'raw sync unaffected');
  check((await raw.promises.readFile(file)).equals(original), 'raw Promise unaffected');
  check(raw.existsSync(file), 'raw exists uses raw access');
  raw.writeFileSync(file + '.written', 'first');
  raw.appendFileSync(file + '.written', '-second');
  check(
    raw.readFileSync(file + '.written', 'utf8') === 'first-second',
    'raw append uses raw write'
  );
} finally {
  Object.assign(fs, {
    readFileSync: saved.readFileSync,
    writeFileSync: saved.writeFileSync,
    accessSync: saved.accessSync
  });
  fs.promises.readFile = saved.readFile;
}

const collect =
  typeof collectHostGarbage === 'function'
    ? collectHostGarbage
    : typeof gc === 'function'
      ? gc
      : undefined;
if (collect) {
  collect();
}
check(readFileSync(file).equals(original), 'raw facade retained through collection');
console.log(
  JSON.stringify({
    bytes: original.length,
    sha256: createHash('sha256').update(original).digest('hex'),
    raw: true,
    descriptors: true,
    patchIsolation: true,
    collected: !!collect
  })
);
if (process.argv[3] === '--firefly') {
  const { app } = await import('firefly');
  app.quit();
}
