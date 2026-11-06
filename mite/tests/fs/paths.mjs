import fs from 'node:fs';
import promises from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const root = 'path é文🪲';
const created = fs.mkdirSync(path.join(root, 'child'), { recursive: true });
const expectedCreated = process.platform === 'win32' ? path.toNamespacedPath(root) : root;
if (created !== expectedCreated) {
  throw Error('Recursive mkdir first-created path');
}
const filename = path.join(root, 'child', 'data é文🪲.txt');
fs.writeFileSync(pathToFileURL(path.resolve(filename)), 'unicode file');
if (fs.readFileSync(Buffer.from(filename), 'utf8') !== 'unicode file') {
  throw Error('URL/Buffer path identity');
}
const rows = [];

function checkError(error, source, dest) {
  const expected = process.platform === 'win32' ? path.resolve(source) : source;
  if (error.code !== 'ENOENT' || error.path !== expected) {
    throw Error('Error source path: ' + JSON.stringify([error.code, error.path, expected]));
  }
  if (
    dest !== undefined &&
    error.dest !== (process.platform === 'win32' ? path.resolve(dest) : dest)
  ) {
    throw Error('Error destination path');
  }
  rows.push([error.code, error.syscall, error.path, error.dest]);
}

for (const name of ['missing.txt', path.join(root, 'missing.txt')]) {
  for (const fn of [
    () => fs.readFileSync(name),
    () => fs.statSync(name),
    () => fs.accessSync(Buffer.from(name))
  ]) {
    let caught = false;
    try {
      fn();
    } catch (error) {
      caught = true;
      checkError(error, name);
    }
    if (!caught) {
      throw Error('Missing path accepted');
    }
  }
  let caught = false;
  try {
    await promises.readFile(pathToFileURL(path.resolve(name)));
  } catch (error) {
    caught = true;
    checkError(error, path.resolve(name));
  }
  if (!caught) {
    throw Error('Missing URL accepted');
  }
  const dest = path.join(root, 'destination.txt');
  try {
    await promises.rename(name, dest);
    throw Error('Missing rename accepted');
  } catch (error) {
    checkError(error, name, dest);
  }
}
const entries = fs.readdirSync(root, { withFileTypes: true });
if (entries.length !== 1 || entries[0].parentPath !== root || entries[0].name !== 'child') {
  throw Error('Dirent parent path changed');
}
console.log(
  JSON.stringify({
    created,
    rows,
    parent: entries[0].parentPath,
    body: await promises.readFile(filename, 'utf8')
  })
);
