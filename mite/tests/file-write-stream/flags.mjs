/** Creation, truncation, append and explicit offsets use genuine filesystem descriptors. */
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import { check, event } from './support.mjs';

async function write(filename, options, value) {
  const stream = fs.createWriteStream(filename, options);
  const closed = event(stream, 'close');
  let error;

  stream.on('error', (value) => {
    error = value;
  });

  stream.end(value);
  await closed;
  check(!error, error?.message);
  return stream;
}

fs.writeFileSync('flags.bin', 'abcdef');
await write('flags.bin', { flags: 'r+', start: 2 }, 'XY');
console.log('offset', fs.readFileSync('flags.bin', 'utf8'));
await write('flags.bin', { flags: 'a' }, 'ä');
console.log('append', fs.readFileSync('flags.bin', 'utf8'));
await write('flags.bin', { flags: 'w' }, 'Z');
console.log('truncate', fs.readFileSync('flags.bin', 'utf8'));
await write('sparse.bin', { start: 3 }, Buffer.from([255]));
console.log('sparse', fs.readFileSync('sparse.bin').toString('hex'));
const filename = pathToFileURL(path.join(process.cwd(), 'unicode-ä.bin'));
const urlStream = await write(filename, 'utf8', '🦋');
check(
  urlStream.path === path.join(process.cwd(), 'unicode-ä.bin'),
  'URL path becomes native pathname'
);
console.log('url', fs.readFileSync(filename).toString('hex'));
