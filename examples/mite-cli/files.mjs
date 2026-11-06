import { readFile, writeFile, stat } from 'node:fs/promises';

const destination = process.argv[2];
if (!destination) {
  console.error('Usage: mite examples/mite-cli/files.mjs <destination-file>');
  process.exit(9);
}
const bytes = await readFile(import.meta.filename);
await writeFile(destination, bytes, { flag: 'wx' });
console.log(`Copied ${bytes.length} bytes; file size is ${(await stat(destination)).size}.`);
