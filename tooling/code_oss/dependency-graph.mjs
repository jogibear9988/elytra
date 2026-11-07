/** Read the pinned upstream graph for inventory, never perform dependency resolution. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import YAML from './node_modules/yaml/dist/index.js';

export const inputNames = [
  'package.json',
  'pnpm-lock.yaml',
  'pnpm-workspace.yaml',
  'pnpmfile.mjs',
  'nativeBuildEnv.ts',
  'foundry-local-sdk.patch',
  'LICENSE.txt'
];

export const digest = (data) => createHash('sha256').update(data).digest('hex');

/** Validate a prepared native-platform tree before copying it into a diagnostic build. */
export function preparedDependencies(root, packageBytes, revision) {
  root = fs.realpathSync(root);
  const report = JSON.parse(fs.readFileSync(path.join(root, 'ELYTRA-DEPENDENCIES.json')));
  if (
    report.kind !== 'code-oss-root-production-dependencies' ||
    report.state !== 'installed-scripts-disabled' ||
    report.revision !== revision ||
    report.lifecycleScripts !== false ||
    report.platform !== process.platform ||
    report.architecture !== process.arch ||
    report.sourceInputs.find((item) => item.path === 'package.json')?.sha256 !==
      digest(packageBytes)
  ) {
    throw new Error('Dependency tree is unaccepted, different-source or different-platform');
  }
  const modules = path.join(root, 'node_modules');

  function links(directory) {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      const filename = path.join(directory, entry.name);
      if (entry.isSymbolicLink()) {
        const target = fs.readlinkSync(filename);
        const relative = path.relative(modules, path.resolve(directory, target));
        if (
          path.isAbsolute(target) ||
          relative === '..' ||
          relative.startsWith(`..${path.sep}`) ||
          path.isAbsolute(relative)
        ) {
          throw new Error(`Dependency link escapes prepared tree: ${filename}`);
        }
      } else if (entry.isDirectory()) {
        links(filename);
      }
    }
  }

  links(modules);
  return { root, report };
}

export function readInputs(source, manifest, flat = false) {
  const inputs = new Map();
  for (const name of inputNames) {
    const entry = manifest.files[name];
    const data = fs.readFileSync(path.join(source, flat ? name : entry.upstream));
    const accepted = [entry.sha256, ...(name === 'LICENSE.txt' ? [entry.crlfCheckoutSha256] : [])];
    if (!accepted.includes(digest(data))) {
      throw new Error(`Pinned dependency input differs: ${entry.upstream}`);
    }
    inputs.set(entry.upstream, data);
  }
  return inputs;
}

export function productionGraph(inputs) {
  const manifest = JSON.parse(inputs.get('package.json'));
  const lock = YAML.parse(inputs.get('pnpm-lock.yaml').toString());
  const workspace = YAML.parse(inputs.get('pnpm-workspace.yaml').toString());
  if (String(lock.lockfileVersion) !== '9.0' || !lock.importers?.['.']) {
    throw new Error('Unsupported upstream lockfile');
  }
  const roots = [];
  const queue = [];
  const packages = new Map();

  function key(name, version) {
    if (typeof version !== 'string' || /^(?:link|file|workspace):/.test(version)) {
      throw new Error(`Uninventoried non-registry dependency: ${name} ${version}`);
    }
    return version.startsWith('npm:') ? version.slice(4) : `${name}@${version}`;
  }

  for (const kind of ['dependencies', 'optionalDependencies']) {
    const declared = manifest[kind] ?? {};
    const locked = lock.importers['.'][kind] ?? {};
    if (
      JSON.stringify(Object.keys(declared).sort()) !== JSON.stringify(Object.keys(locked).sort())
    ) {
      throw new Error(`Root ${kind} mismatch`);
    }
    for (const [name, specifier] of Object.entries(declared)) {
      const entry = locked[name];
      if (entry.specifier !== specifier) {
        throw new Error(`Changed upstream dependency specifier: ${name}`);
      }
      const snapshot = key(name, entry.version);
      roots.push({
        name,
        specifier,
        version: entry.version,
        snapshot,
        optional: kind === 'optionalDependencies'
      });
      queue.push(snapshot);
    }
  }
  while (queue.length) {
    const snapshot = queue.pop();
    if (packages.has(snapshot)) {
      continue;
    }
    const entry = lock.snapshots?.[snapshot];
    const base = snapshot.split('(')[0];
    const metadata = lock.packages?.[base];
    if (!entry || !metadata?.resolution?.integrity) {
      throw new Error(`Missing integrity-pinned snapshot: ${snapshot}`);
    }
    const separator = base.lastIndexOf('@');
    const name = base.slice(0, separator);
    const version = base.slice(separator + 1);
    const edges = [];
    for (const kind of ['dependencies', 'optionalDependencies']) {
      for (const [child, selected] of Object.entries(entry[kind] ?? {})) {
        const next = key(child, selected);
        edges.push({ name: child, snapshot: next, optional: kind === 'optionalDependencies' });
        queue.push(next);
      }
    }
    const upstreamBuildPolicy = workspace.allowBuilds?.[base] ?? workspace.allowBuilds?.[name];
    packages.set(snapshot, {
      snapshot,
      name,
      version,
      integrity: metadata.resolution.integrity,
      os: metadata.os,
      cpu: metadata.cpu,
      upstreamBuildPolicy,
      edges
    });
  }
  return {
    packageManager: manifest.packageManager,
    nativeBuildTargetUpstream: manifest.nativeBuild,
    roots: roots.sort((a, b) => a.name.localeCompare(b.name)),
    packages: [...packages.values()].sort((a, b) => a.snapshot.localeCompare(b.snapshot)),
    omittedScopes: [
      'root devDependencies',
      'other workspace importers',
      'separate install roots/extensions'
    ]
  };
}

/** Inspect installed data without loading any package or executing native code. */
export function installedInventory(root) {
  const packages = [];

  function visitModules(directory) {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      if (entry.name.startsWith('.')) {
        continue;
      }
      const filename = path.join(directory, entry.name);
      if (entry.name.startsWith('@') && entry.isDirectory()) {
        visitModules(filename);
        continue;
      }
      if (!entry.isDirectory()) {
        continue;
      }
      const json = path.join(filename, 'package.json');
      if (!fs.existsSync(json)) {
        continue;
      }
      const bytes = fs.readFileSync(json);
      const manifest = JSON.parse(bytes);
      const markers = [];
      let fileBytes = 0;

      function files(location) {
        for (const file of fs.readdirSync(location, { withFileTypes: true })) {
          if (file.name === 'node_modules') {
            continue;
          }
          const absolute = path.join(location, file.name);
          if (file.isDirectory()) {
            files(absolute);
          } else if (file.isFile()) {
            fileBytes += fs.statSync(absolute).size;
            if (file.name === 'binding.gyp' || /\.(node|wasm|dll|dylib|so|exe)$/.test(file.name)) {
              markers.push(path.relative(filename, absolute));
            }
          }
        }
      }

      files(filename);
      const lifecycleScripts = Object.fromEntries(
        Object.entries(manifest.scripts ?? {}).filter(([name]) =>
          ['preinstall', 'install', 'postinstall'].includes(name)
        )
      );
      const nativeLoaderDependencies = Object.keys({
        ...manifest.dependencies,
        ...manifest.optionalDependencies
      }).filter((name) =>
        [
          'node-addon-api',
          'nan',
          'node-gyp',
          'node-gyp-build',
          'prebuild-install',
          'cmake-js'
        ].includes(name)
      );
      packages.push({
        path: path.relative(root, filename),
        name: manifest.name,
        version: manifest.version,
        manifestSha256: digest(bytes),
        lifecycleScripts,
        nativeLoaderDependencies,
        markers,
        logicalFileBytes: fileBytes
      });
      const nested = path.join(filename, 'node_modules');
      if (fs.existsSync(nested)) {
        visitModules(nested);
      }
    }
  }

  visitModules(path.join(root, 'node_modules'));

  packages.sort((a, b) => a.path.localeCompare(b.path));

  return {
    packages,
    candidates: packages.filter(
      (item) =>
        item.markers.length ||
        item.nativeLoaderDependencies.length ||
        Object.values(item.lifecycleScripts).some((script) =>
          /node-gyp|prebuild|cmake-js|napi/i.test(script)
        )
    ),
    classification:
      'Evidence candidates only; absence of markers does not certify compatibility. No package was executed.'
  };
}
