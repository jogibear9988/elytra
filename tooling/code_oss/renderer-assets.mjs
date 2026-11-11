/** Stage the pinned upstream dev renderer, retaining its real CSS import-map loader. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

const extensions = new Set([
  '.html',
  '.css',
  '.svg',
  '.png',
  '.webp',
  '.mp3',
  '.woff',
  '.woff2',
  '.ttf',
  '.wasm',
  '.qwoff',
  '.ico',
  '.icns'
]);
const excluded = new Set(['test', 'tests', 'fixtures', '__snapshots__']);

export function runtimeAsset(name) {
  return (
    extensions.has(path.extname(name).toLowerCase()) &&
    !name.split('/').some((part) => excluded.has(part))
  );
}

export function rendererAssets(source, dependencies, paths, planned, inputs) {
  if (!dependencies) {
    throw Error('--renderer-assets requires verified prepared dependencies');
  }
  for (const name of [
    'src/vs/code/electron-browser/workbench/workbench-dev.html',
    'src/vs/base/browser/ui/codicons/codicon/codicon.css'
  ]) {
    if (!planned.has(name)) {
      throw Error(`Pinned renderer input missing: ${name}`);
    }
  }
  const resources = path.join(source, 'resources');

  function collect(directory) {
    for (const entry of fs.readdirSync(path.join(source, directory), { withFileTypes: true })) {
      const name = `${directory}/${entry.name}`;
      if (entry.name === '.DS_Store') {
        continue;
      }
      if (entry.isSymbolicLink()) {
        throw Error(`Renderer resource symlink requires review: ${name}`);
      }
      if (entry.isDirectory()) {
        collect(name);
      } else if (runtimeAsset(name)) {
        const bytes = fs.readFileSync(path.join(source, name));
        planned.set(name, bytes);
        inputs.push({
          source: name,
          sourceSha256: digest(bytes),
          output: name,
          outputSha256: digest(bytes)
        });
        paths.add(name);
      }
    }
  }

  if (fs.existsSync(resources)) {
    collect('resources');
  }
  const packageRoot = path.join(dependencies.root, 'node_modules/@vscode/codicons');
  const packageBytes = fs.readFileSync(path.join(packageRoot, 'package.json'));
  const manifest = JSON.parse(packageBytes);
  if (manifest.name !== '@vscode/codicons') {
    throw Error('Unexpected codicon font package');
  }
  const font = fs.readFileSync(path.join(packageRoot, 'dist/codicon.ttf'));
  const target = 'src/vs/base/browser/ui/codicons/codicon/codicon.ttf';
  if (planned.has(target) && !planned.get(target).equals(font)) {
    throw Error('Codicon font source/package drift');
  }
  planned.set(target, font);
  const assetInputs = inputs.filter((record) => runtimeAsset(record.source));
  const bytes = assetInputs.reduce(
    (sum, record) => sum + planned.get(record.output).length,
    font.length
  );
  if (assetInputs.length > 2000 || bytes > 64 * 1024 * 1024) {
    throw Error('Review renderer asset count/byte budget');
  }
  const cssModules = [...planned.keys()]
    .filter((name) => name.startsWith('src/vs/') && name.endsWith('.css'))
    .map((name) => name.slice(4))
    .sort();
  if (!cssModules.length || cssModules.length > 2048) {
    throw Error('Review staged CSS inventory count');
  }
  const cssInventory = Buffer.from(
    JSON.stringify(
      {
        schema: 1,
        kind: 'code-oss-staged-dev-css',
        revision: dependencies.report.revision,
        cssModules
      },
      null,
      2
    ) + '\n'
  );
  const cssTarget = 'src/elytra-css-modules.json';
  if (planned.has(cssTarget)) {
    throw Error('Staged CSS inventory output collision');
  }
  planned.set(cssTarget, cssInventory);
  const generated = [
    {
      output: cssTarget,
      outputSha256: digest(cssInventory),
      source: 'compiler-staged-css-output-paths'
    }
  ];
  if (!inputs.some((record) => record.output === target)) {
    generated.push({
      output: target,
      outputSha256: digest(font),
      package: manifest.name,
      version: manifest.version,
      packageSha256: digest(packageBytes),
      source: 'node_modules/@vscode/codicons/dist/codicon.ttf'
    });
  }
  return {
    schema: 1,
    mode: 'upstream-dev-css-import-map',
    files: assetInputs.length,
    bytes,
    aliases: [{ path: 'out', target: 'src' }],
    cssModules: cssModules.length,
    generated,
    status: 'assets staged; real workbench startup remains unverified'
  };
}
