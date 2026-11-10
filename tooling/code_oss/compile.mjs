/**
 * Build-only compilation of a diagnostic Code-OSS source tree. Preserve real
 * imports and application algorithms; this does not install packages or
 * type-check the project or claim a distributable app. Optional renderer assets
 * retain the upstream dev CSS import-map path, with verified in-place refresh.
 */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import { transformSync, version } from '../runtime-js/node_modules/esbuild/lib/main.js';
import { preparedDependencies } from './dependency-graph.mjs';
import { createNLS } from './nls.mjs';
import { verifiedRefresh, checkRefreshPlan } from './compile-refresh.mjs';
import { runtimeAsset, rendererAssets } from './renderer-assets.mjs';
import { planPolicyWatcher, applyPolicyWatcher } from './policy-watcher.mjs';
import { planNativeKeymap, applyNativeKeymap } from './native-keymap.mjs';
import { planSQLite, applySQLite } from './sqlite.mjs';
import { verifyPackageAdditions } from './package-adaptations.mjs';

const { values } = parseArgs({
  options: {
    source: { type: 'string' },
    output: { type: 'string' },
    overlay: { type: 'string' },
    dependencies: { type: 'string' },
    nls: { type: 'boolean', default: false },
    refresh: { type: 'boolean', default: false },
    'renderer-assets': { type: 'boolean', default: false },
    'mite-policy-watcher': { type: 'boolean', default: false },
    'mite-native-keymap': { type: 'boolean', default: false },
    'mite-sqlite': { type: 'boolean', default: false }
  }
});
if (!values.source || !values.output || version !== '0.25.12') {
  throw new Error('Require --source, --output and pinned esbuild 0.25.12');
}
const repository = fileURLToPath(new URL('../../', import.meta.url));
const pin = JSON.parse(
  fs.readFileSync(path.join(repository, 'vendor/code-oss-tests/manifest.json'))
);
const source = fs.realpathSync(values.source);
const output = path.join(
  fs.realpathSync(path.dirname(path.resolve(values.output))),
  path.basename(values.output)
);
const overlay = values.overlay && fs.realpathSync(values.overlay);

const sha256 = (data) => createHash('sha256').update(data).digest('hex');

function within(parent, child) {
  const relative = path.relative(parent, child);
  return (
    !relative ||
    (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative))
  );
}

for (const input of [source, overlay].filter(Boolean)) {
  if (within(input, output) || within(output, input)) {
    throw new Error('Output overlaps input tree');
  }
}
let refresh;
if (fs.existsSync(output)) {
  if (!values.refresh) {
    throw new Error('Output must be fresh (or use verified --refresh)');
  }
  refresh = verifiedRefresh(output);
} else if (values.refresh) {
  throw Error('--refresh requires an existing owned compiled tree');
}
const replacements = new Map();
let adaptations;
if (overlay) {
  adaptations = JSON.parse(fs.readFileSync(path.join(overlay, 'ELYTRA-ADAPTATIONS.json'), 'utf8'));
  if (adaptations.schema !== 1 || adaptations.kind !== 'partial-source-overlay') {
    throw new Error('Unknown overlay');
  }
  for (const record of adaptations.files) {
    if (
      path.isAbsolute(record.path) ||
      record.path.split(/[\\/]/).some((part) => !part || part === '..' || part === '.')
    ) {
      throw new Error('Invalid overlay path');
    }
    const original = record.ownerSource
      ? path.resolve(repository, record.ownerSource)
      : path.join(source, record.path);
    if (record.ownerSource && !within(repository, original)) {
      throw new Error('Invalid owned source path');
    }
    const data = fs.readFileSync(path.join(overlay, record.path));
    if (
      sha256(fs.readFileSync(original)) !== record.sourceSha256 ||
      sha256(data) !== record.overlaySha256
    ) {
      throw new Error(`Overlay input/output drift: ${record.path}`);
    }
    replacements.set(record.path, data);
  }
}
const paths = new Set();

function collect(directory) {
  for (const entry of fs.readdirSync(path.join(source, directory), { withFileTypes: true })) {
    const relative = `${directory}/${entry.name}`;
    if (entry.name === '.DS_Store') {
      continue;
    }
    if (entry.isSymbolicLink()) {
      throw new Error(`Source symlink requires explicit handling: ${relative}`);
    }
    if (entry.isDirectory()) {
      collect(relative);
    } else if (
      (/\.(ts|js|json)$/.test(relative) && !relative.endsWith('.d.ts')) ||
      (values['renderer-assets'] && runtimeAsset(relative))
    ) {
      paths.add(relative);
    }
  }
}

collect('src');
for (const name of replacements.keys()) {
  if (name.startsWith('src/')) {
    paths.add(name);
  }
}
const planned = new Map();
const inputs = [];
const nls = values.nls ? await createNLS() : undefined;
for (const name of [...paths].sort()) {
  const data = replacements.get(name) ?? fs.readFileSync(path.join(source, name));
  const text =
    nls && /\.(?:ts|js)$/.test(name)
      ? nls.patch(name, data.toString('utf8'))
      : data.toString('utf8');
  const compiled = name.endsWith('.ts')
    ? Buffer.from(
        transformSync(text, {
          loader: 'ts',
          format: 'esm',
          target: 'es2024',
          legalComments: 'inline',
          tsconfigRaw: {
            compilerOptions: { experimentalDecorators: true, useDefineForClassFields: false }
          }
        }).code
      )
    : nls && name.endsWith('.js')
      ? Buffer.from(text)
      : data;
  const target = name.replace(/\.ts$/, '.js');
  if (planned.has(target)) {
    throw new Error(`Duplicate compilation output: ${target}`);
  }
  planned.set(target, compiled);
  inputs.push({
    source: name,
    sourceSha256: sha256(data),
    output: target,
    outputSha256: sha256(compiled)
  });
}
if (nls) {
  for (const [name, data] of nls.artifacts()) {
    if (planned.has(name)) {
      throw new Error(`Generated NLS output collision: ${name}`);
    }
    planned.set(name, data);
  }
}
for (const name of ['LICENSE.txt', 'ThirdPartyNotices.txt', 'package.json', 'product.json']) {
  if (fs.existsSync(path.join(source, name))) {
    planned.set(name, fs.readFileSync(path.join(source, name)));
  }
}
if (!planned.has('package.json')) {
  planned.set('package.json', Buffer.from('{"type":"module"}\n'));
}
if (JSON.parse(planned.get('package.json')).type !== 'module') {
  throw new Error('Expected Code-OSS ESM package');
}
const dependencies =
  values.dependencies &&
  preparedDependencies(values.dependencies, planned.get('package.json'), pin.revision);
const packageSelectors = [
  ['mite-policy-watcher', planPolicyWatcher, applyPolicyWatcher],
  ['mite-native-keymap', planNativeKeymap, applyNativeKeymap],
  ['mite-sqlite', planSQLite, applySQLite]
];
if (packageSelectors.some(([option]) => values[option]) && !dependencies) {
  throw Error('Native package adapters require a verified prepared dependency tree');
}
const packagePlans = packageSelectors
  .filter(([option]) => values[option])
  .map(([, plan, apply]) => ({ plan: plan(dependencies.root), apply }));
let packageAdditions = packagePlans;
const assets = values['renderer-assets']
  ? rendererAssets(source, dependencies, paths, planned, inputs)
  : undefined;
if (
  refresh &&
  (!!refresh.previous.nls !== !!nls || (!!refresh.previous.rendererAssets && !assets))
) {
  throw Error('Refresh must preserve existing NLS/renderer build modes');
}
if (refresh && !!refresh.previous.runtimeDependencies !== !!dependencies) {
  throw Error('Refresh must preserve prepared dependency mode');
}
if (refresh && dependencies) {
  const old = refresh.previous.runtimeDependencies;
  if (
    !old ||
    old.lockSha256 !==
      dependencies.report.sourceInputs.find((item) => item.path === 'pnpm-lock.yaml')?.sha256 ||
    old.revision !== pin.revision ||
    !fs.existsSync(path.join(output, 'node_modules'))
  ) {
    throw Error('Refresh dependency graph differs');
  }
  // Verify every old adapter and each newly selected ORIGINAL loader+manifest
  // before any stage write. Removal or modification of an old adapter is refused.
  packageAdditions = verifyPackageAdditions(
    output,
    refresh.previous.packageAdaptations ?? [],
    packagePlans
  );
}
if (dependencies && (within(dependencies.root, output) || within(output, dependencies.root))) {
  throw new Error('Output overlaps dependency input tree');
}
planned.set('ELYTRA-LICENSE.txt', fs.readFileSync(path.join(repository, 'LICENSE')));
planned.set(
  'ELYTRA-COMPILE.json',
  Buffer.from(
    JSON.stringify(
      {
        schema: 1,
        kind: 'diagnostic-source-tree',
        compiler: `esbuild ${version}`,
        runtimeDependencies: dependencies
          ? {
              revision: dependencies.report.revision,
              state: dependencies.report.state,
              lockSha256: dependencies.report.sourceInputs.find(
                (item) => item.path === 'pnpm-lock.yaml'
              )?.sha256,
              installedPackageCount: dependencies.report.installed.packages.length
            }
          : undefined,
        adaptations,
        rendererAssets: assets,
        refreshedFromSha256: refresh?.manifestSha256,
        rootFiles: [
          'LICENSE.txt',
          'ThirdPartyNotices.txt',
          'package.json',
          'product.json',
          'ELYTRA-LICENSE.txt'
        ]
          .filter((name) => planned.has(name))
          .map((output) => ({ output, outputSha256: sha256(planned.get(output)) })),
        packageAdaptations: packagePlans.length
          ? packagePlans.map(({ plan }) => plan.record)
          : undefined,
        nls: nls?.report(),
        generated: nls
          ? [...nls.artifacts()].map(([output, data]) => ({ output, outputSha256: sha256(data) }))
          : undefined,
        inputs
      },
      null,
      2
    ) + '\n'
  )
);
// All transforms, aliases and refresh checks finish before any write.
if (assets && fs.lstatSync(path.join(output, 'out'), { throwIfNoEntry: false })) {
  const alias = path.join(output, 'out');
  if (!fs.lstatSync(alias).isSymbolicLink() || fs.readlinkSync(alias) !== 'src') {
    throw Error('Renderer out alias conflicts with existing output');
  }
}

if (refresh) {
  checkRefreshPlan(output, refresh, planned);
  // A killed refresh must not retain an apparently completed old build record.
  fs.renameSync(
    path.join(output, 'ELYTRA-COMPILE.json'),
    path.join(output, 'ELYTRA-COMPILE.previous.json')
  );
} else {
  fs.mkdirSync(output);
}
for (const [name, data] of planned) {
  if (name === 'ELYTRA-COMPILE.json') {
    continue;
  }
  const destination = path.join(output, name);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  if (!fs.existsSync(destination) || !fs.readFileSync(destination).equals(data)) {
    fs.writeFileSync(destination, data);
  }
}
if (dependencies && !refresh) {
  fs.cpSync(path.join(dependencies.root, 'node_modules'), path.join(output, 'node_modules'), {
    recursive: true,
    dereference: false,
    verbatimSymlinks: true
  });
  fs.copyFileSync(
    path.join(dependencies.root, 'ELYTRA-DEPENDENCIES.json'),
    path.join(output, 'ELYTRA-DEPENDENCIES.json')
  );
}
if (dependencies) {
  if (!refresh) {
    packageAdditions = verifyPackageAdditions(output, [], packagePlans);
  }
  for (const { plan, apply } of packageAdditions) {
    apply(output, plan);
  }
}
if (assets && !fs.existsSync(path.join(output, 'out'))) {
  // Relative and internal: preserve one physical module tree. Failure on a host
  // without symlink privilege is explicit; no Windows acceptance is implied.
  fs.symlinkSync('src', path.join(output, 'out'), 'dir');
}
// A killed or failed copy must not leave an apparently completed build record.
fs.writeFileSync(path.join(output, 'ELYTRA-COMPILE.json'), planned.get('ELYTRA-COMPILE.json'));
console.log(`Compiled ${inputs.length} real source files into ${output}`);
