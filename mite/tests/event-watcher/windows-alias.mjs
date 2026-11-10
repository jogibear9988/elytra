/** Real Windows 8.3/long aliases must produce identical relative recursive filenames. */
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

if (process.platform !== 'win32') {
  throw Error('Require genuine Windows short/long aliases from the native runner');
}
const [short, long] = process.argv.slice(2);
if (!short || !long || short.toLowerCase() === long.toLowerCase()) {
  throw Error('Require two distinct native aliases');
}
const rootName = 'initial é文.txt';
const nestedName = path.join('nested', 'child é文.txt');
fs.mkdirSync(path.join(long, 'nested'));
fs.writeFileSync(path.join(long, rootName), 'base');
fs.writeFileSync(path.join(long, nestedName), 'base');
const observations = [new Set(), new Set()];
const watchers = [short, pathToFileURL(long)].map((name, index) => {
  const watcher = fs.watch(name, { recursive: true }, (kind, filename) => {
    if (kind !== 'rename' && kind !== 'change') {
      throw Error('Unexpected native alias event type');
    }
    if (filename !== null) {
      observations[index].add(filename);
    }
  });

  watcher.on('error', (error) => {
    throw error;
  });

  return watcher;
});
try {
  await new Promise((resolve) => setTimeout(resolve, 50));
  fs.appendFileSync(path.join(long, rootName), '-change');
  fs.appendFileSync(path.join(long, nestedName), '-change');
  const deadline = Date.now() + 3500;
  while (
    !observations.every((names) => names.has(rootName) && names.has(nestedName)) &&
    Date.now() < deadline
  ) {
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
  if (!observations.every((names) => names.has(rootName) && names.has(nestedName))) {
    throw Error(
      'Native short/long alias relative names: ' +
        JSON.stringify(observations.map((names) => [...names]))
    );
  }
} finally {
  for (const watcher of watchers) {
    watcher.close();
  }
}
console.log('PASS actual Windows short/long aliases preserve Unicode root and nested filenames');
