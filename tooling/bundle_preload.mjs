// Build tool only: authored ESM becomes the sandbox's supported CommonJS payload.
import { readFile, mkdir } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const [source, destination, ...extra] = process.argv.slice(2);
if (!source || !destination || extra.length || path.resolve(source) === path.resolve(destination)) {
  throw Error('Usage: node tooling/bundle_preload.mjs source.mjs output.cjs');
}
const manifestURL = new URL('./runtime-js/package.json', import.meta.url);
const manifest = JSON.parse(await readFile(manifestURL, 'utf8'));
const { build, version } = createRequire(manifestURL)('esbuild');
if (version !== manifest.devDependencies.esbuild) {
  throw Error('Install the pinned tooling/runtime-js lockfile first');
}
await mkdir(path.dirname(path.resolve(destination)), { recursive: true });
await build({
  entryPoints: [path.resolve(source)],
  outfile: path.resolve(destination),
  bundle: true,
  format: 'cjs',
  platform: 'browser',
  target: 'es2022',
  external: ['firefly', 'electron'],
  legalComments: 'eof',
  logLevel: 'warning'
});
