/** Genuine native recursive backend only on the platforms admitted by Mite. */
import fs from 'node:fs';
import path from 'node:path';
if (process.platform !== 'darwin' && process.platform !== 'win32') {
  console.log('SKIP native recursive watcher fixture on this platform');
} else {
  fs.mkdirSync('recursive/nested', { recursive: true });
  fs.writeFileSync('recursive/nested/child.txt', 'baseline');
  let observed = false;
  const watcher = fs.watch('recursive', { recursive: true, persistent: false }, (kind, name) => {
    if (kind !== 'change' && kind !== 'rename') {
      throw Error('Invalid recursive event');
    }
    if (name !== null && name.split(path.sep).join('/') === 'nested/child.txt') {
      observed = true;
    }
  });

  watcher.on('error', (error) => {
    throw error;
  });

  fs.appendFileSync('recursive/nested/child.txt', '-changed');
  const deadline = Date.now() + 3500;
  while (!observed && Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
  watcher.close();
  if (!observed) {
    throw Error('Actual nested native event with relative filename missing');
  }
  console.log('PASS native recursive nested event and relative filename');
}
