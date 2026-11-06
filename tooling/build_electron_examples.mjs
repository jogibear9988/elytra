import { readFile, writeFile, mkdir, cp, access } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const [destination, runtime = 'firefly'] = process.argv.slice(2);
if (!destination || !['firefly', 'electron'].includes(runtime))
  throw Error('Usage: node tooling/build_electron_examples.mjs fresh-output [firefly|electron]');
const source = fileURLToPath(new URL('../examples/electron/', import.meta.url));
const output = path.resolve(destination);
try {
  await access(output);
  throw Error('Output exists; choose a fresh build directory');
} catch (error) {
  if (error.code !== 'ENOENT') throw error;
}
const manifest = JSON.parse(
  await readFile(new URL('./runtime-js/package.json', import.meta.url), 'utf8')
);
const { build, version } = createRequire(new URL('./runtime-js/package.json', import.meta.url))(
  'esbuild'
);
if (version !== manifest.devDependencies.esbuild)
  throw Error('Install the pinned tooling/runtime-js lockfile');
await mkdir(output, { recursive: true });
await cp(source, output, { recursive: true });
const catalog = JSON.parse(await readFile(path.join(source, 'catalog.json'), 'utf8'));
for (const entry of catalog.examples) {
  if (runtime === 'electron') {
    const file = path.join(output, entry.id, 'main.mjs');
    const main = await readFile(file, 'utf8');
    await writeFile(file, main.replaceAll("from 'firefly'", "from 'electron'"));
  }
  if (!entry.preload) continue;
  await build({
    entryPoints: [path.join(source, 'shared/preload.mjs')],
    outfile: path.join(output, entry.id, 'preload.cjs'),
    bundle: true,
    format: 'cjs',
    platform: 'browser',
    target: 'es2022',
    external: ['firefly', 'electron'],
    alias: runtime === 'electron' ? { firefly: 'electron' } : {},
    define: {
      __EXAMPLE_ID__: JSON.stringify(entry.id),
      __EXAMPLE_ACTIONS__: JSON.stringify(entry.actions),
      __EXAMPLE_CHANNEL__: JSON.stringify('elytra-example:' + entry.id)
    },
    legalComments: 'eof'
  });
}
await writeFile(
  path.join(output, 'runtime.json'),
  JSON.stringify(
    { runtime, source: 'examples/electron', reviewed: catalog.reviewed, bundler: version },
    null,
    2
  ) + '\n'
);
console.log(
  `Built ${catalog.examples.length} ESM example apps for explicit ${runtime} runtime at ${output}`
);
