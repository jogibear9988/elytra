/** Generate Firefly's native API/DOM script adapters from maintained ESM. */
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import * as esbuild from 'esbuild';
import { compileFactory } from './factories.mjs';

const repository = path.resolve(import.meta.dirname, '../..');
const destination = path.join(repository, 'firefly/core/generated');
const check = process.argv.includes('--check');

const digest = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');

const entries = {
  'api.js': { source: 'firefly/core/api.mjs', eager: false },
  'safe-storage.js': { source: 'firefly/core/safe-storage/index.mjs', eager: false },
  'utility-parent.js': { source: 'firefly/core/utility-process/parent-port.mjs', eager: false },
  'app-regions.js': { source: 'firefly/native/qt/app-regions.mjs', eager: true }
};
const manifest = { generator: `esbuild ${esbuild.version}`, files: {}, sources: {} };
const inputs = new Set([
  'tooling/runtime-js/build-firefly.mjs',
  'tooling/runtime-js/factories.mjs',
  'tooling/runtime-js/package.json',
  'tooling/runtime-js/package-lock.json'
]);
const pkg = JSON.parse(
  await fs.readFile(path.join(repository, 'tooling/runtime-js/package.json'), 'utf8')
);
if (esbuild.version !== pkg.devDependencies.esbuild) {
  throw Error('Install the pinned runtime-js lockfile');
}
if (!check) {
  await fs.mkdir(destination, { recursive: true });
}

async function save(name, source) {
  const file = path.join(destination, name);
  let previous;
  try {
    previous = await fs.readFile(file, 'utf8');
  } catch (error) {
    if (error.code !== 'ENOENT') {
      throw error;
    }
  }
  if (check && previous !== source) {
    throw Error('Stale Firefly generated source: ' + name);
  }
  if (!check && previous !== source) {
    await fs.writeFile(file, source);
  }
}

for (const [name, entry] of Object.entries(entries)) {
  const compiled = await compileFactory(repository, entry.source);
  const source = entry.eager ? compiled.source.trimEnd() + '();\n' : compiled.source;
  await save(name, source);
  manifest.files[name] = digest(source);
  for (const input of compiled.inputs) {
    if (!input.startsWith('firefly/')) {
      throw Error('Unexpected Firefly source input');
    }
    inputs.add(input);
  }
}
for (const input of [...inputs].sort()) {
  manifest.sources[input] = digest(await fs.readFile(path.join(repository, input)));
}
await save('manifest.json', JSON.stringify(manifest, null, 2) + '\n');
console.log(check ? 'Firefly ESM adapters reproduce exactly.' : 'Generated Firefly ESM adapters.');
