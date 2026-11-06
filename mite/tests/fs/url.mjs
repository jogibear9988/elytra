import fs from 'node:fs';
import { fileURLToPath, pathToFileURL, fileURLToPathBuffer } from 'node:url';
const directory = pathToFileURL(process.cwd() + '/');
const file = new URL('URL%20%C3%A9%25.txt', directory);
file.search = '?ignored';
file.hash = '#ignored';
fs.writeFileSync(file, 'file URL');
console.log(fs.readFileSync(file, 'utf8'), fs.readFileSync(fileURLToPathBuffer(file), 'utf8'));
console.log((await fs.promises.stat(file)).size);
const handle = await fs.promises.open(file, 'r');
console.log(await handle.readFile('utf8'));
await handle.close();
console.log(
  fs
    .readdirSync(directory, { withFileTypes: true })
    .every((entry) => entry.parentPath === fileURLToPath(directory))
);
const renamed = new URL('renamed.txt', directory);
await fs.promises.rename(file, renamed);
console.log(fs.existsSync(renamed), fs.existsSync(file));
for (const bad of [
  new URL('https://example.org/a'),
  new URL('file:///a%2fb'),
  new URL('file:///a%00b')
]) {
  try {
    fs.readFileSync(bad);
  } catch (error) {
    console.log(error.name, error.code);
  }
}
const Original = URL;

globalThis.URL = function Replacement() {};

console.log(fs.readFileSync(renamed, 'utf8'), renamed instanceof Original);
await fs.promises.unlink(renamed);
