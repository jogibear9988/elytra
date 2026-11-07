/** Prepare the exact root production install in a fresh tree, using upstream pnpm. */
import fs from 'node:fs';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { readInputs, productionGraph, installedInventory, digest } from './dependency-graph.mjs';

const { values } = parseArgs({
  options: {
    source: { type: 'string' },
    output: { type: 'string' },
    'inventory-only': { type: 'boolean', default: false }
  }
});
if (!values.output) {
  throw new Error('Require fresh --output; optional --source selects a full upstream checkout');
}
const repository = fileURLToPath(new URL('../../', import.meta.url));
const manifest = JSON.parse(
  fs.readFileSync(path.join(repository, 'vendor/code-oss-tests/manifest.json'))
);
const flat = !values.source;
const source = fs.realpathSync(values.source ?? path.join(repository, 'vendor/code-oss-tests'));
const output = path.join(
  fs.realpathSync(path.dirname(path.resolve(values.output))),
  path.basename(values.output)
);

function overlaps(parent, child) {
  const relative = path.relative(parent, child);
  return (
    !relative ||
    (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative))
  );
}

if (overlaps(source, output) || overlaps(output, source)) {
  throw new Error('Dependency output overlaps source');
}
try {
  fs.lstatSync(output);
  throw new Error('Dependency output must be fresh');
} catch (error) {
  if (error.code !== 'ENOENT') {
    throw error;
  }
}
const inputs = readInputs(source, manifest, flat);
const graph = productionGraph(inputs);
const pnpm = JSON.parse(
  fs.readFileSync(new URL('./node_modules/pnpm/package.json', import.meta.url))
);
if (graph.packageManager !== `pnpm@${pnpm.version}` || pnpm.version !== '12.16.0') {
  throw new Error('Wrong upstream package manager');
}
const command = [
  fileURLToPath(new URL('./node_modules/pnpm/bin/pnpm.mjs', import.meta.url)),
  'install',
  '--prod',
  '--ignore-scripts',
  '--frozen-lockfile',
  '--filter',
  '.',
  '--store-dir',
  path.join(repository, 'build/code-oss-pnpm-store')
];
if (!fs.existsSync(command[0])) {
  throw new Error('Pinned package-manager entry is missing');
}
const report = {
  schema: 1,
  kind: 'code-oss-root-production-dependencies',
  revision: manifest.revision,
  state: 'prepared-not-installed',
  lifecycleScripts: false,
  runtimeCompatibility: 'not-established',
  platform: process.platform,
  architecture: process.arch,
  graph,
  sourceInputs: [...inputs].map(([name, bytes]) => ({ path: name, sha256: digest(bytes) })),
  packageManager: graph.packageManager,
  command: [process.execPath, ...command]
};
// Validate every pinned input and the graph before creating output.
fs.mkdirSync(output);
for (const [name, bytes] of inputs) {
  const target = path.join(output, name);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, bytes);
}

const record = () =>
  fs.writeFileSync(
    path.join(output, 'ELYTRA-DEPENDENCIES.json'),
    JSON.stringify(report, null, 2) + '\n'
  );

record();
if (!values['inventory-only']) {
  const result = spawnSync(process.execPath, command, { cwd: output, stdio: 'inherit' });
  if (result.status !== 0 || result.error) {
    report.state = 'install-failed';
    report.exitStatus = result.status;
    report.signal = result.signal;
    record();
    throw new Error('Pinned root dependency installation failed; inspect ELYTRA-DEPENDENCIES.json');
  }
  for (const [name, bytes] of inputs) {
    if (digest(fs.readFileSync(path.join(output, name))) !== digest(bytes)) {
      report.state = 'installed-input-drift';
      record();
      throw new Error(`Installed input drift: ${name}`);
    }
  }
  readInputs(source, manifest, flat);
  report.installed = installedInventory(output);
  report.state = 'installed-scripts-disabled';
  record();
}
console.log(
  `Code-OSS root graph: ${graph.roots.length} direct dependencies, ${graph.packages.length} locked snapshots; ${report.state}`
);
