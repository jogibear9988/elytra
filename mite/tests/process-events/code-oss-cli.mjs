/** Actual pinned CLI parser with its real locked npm dependency; no desktop simulation. */
import { pathToFileURL } from 'node:url';
const { parseCLIArgs } = await import(pathToFileURL(process.argv[2]).href);
const original = process.argv;
const cases = [
  [],
  ['file.txt'],
  ['--locale', 'fr-fr'],
  ['--user-data-dir', '/tmp/a b # Grüße'],
  ['--js-flags=--expose-gc'],
  ['--no-sandbox'],
  ['--sandbox=false'],
  ['--disable-chromium-sandbox'],
  ['--crash-reporter-directory=/tmp/crashes'],
  ['--', '--locale', 'de'],
  ['--locale=fr', '--locale=de'],
  ['-n', '3'],
  ['--__proto__.polluted', 'yes'],
  ['--constructor.prototype.polluted=yes']
];
try {
  for (const args of cases) {
    process.argv = ['runtime', 'entry', ...args];
    const parsed = parseCLIArgs();
    if (Object.prototype.polluted !== undefined) {
      throw new Error('CLI prototype pollution');
    }
    console.log(JSON.stringify(parsed));
  }
} finally {
  process.argv = original;
}
