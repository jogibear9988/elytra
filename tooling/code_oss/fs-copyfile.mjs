/** Exact fs-copyfile2 adaptation to Mite's real libuv copy and native volume query. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
const repository = fileURLToPath(new URL('../../', import.meta.url));
const pin = JSON.parse(fs.readFileSync(path.join(repository, 'vendor/fs-copyfile/manifest.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const sourceHash = (name) => pin.files.find((file) => file.path === name).sha256;

const actual =
  "typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined";

export function transformFsCopyfile(bytes, name) {
  if (hash(bytes) !== sourceHash(name)) {
    throw Error('Fs-copyfile2 source drift');
  }
  let text = bytes.toString();
  if (name === 'lib/native.js') {
    const load = 'let binding;';
    if (text.split(load).length !== 2 || text.split('if (isMacOS) {').length !== 2) {
      throw Error('Fs-copyfile native seam drift');
    }
    text = text
      .replace(
        load,
        `const __elytraMiteCopy = ${actual} ? await import('mite:fs-copyfile') : undefined;\nlet binding;`
      )
      .replace('if (isMacOS) {', 'if (isMacOS && !__elytraMiteCopy) {');
    for (const key of ['copyFile', 'copyFileSync', 'isCloneSupported', 'constants']) {
      const prefix = `export const ${key} = `;
      if (text.split(prefix).length !== 2) {
        throw Error('Fs-copyfile export seam drift');
      }
      text = text.replace(prefix, prefix + `__elytraMiteCopy ? __elytraMiteCopy.${key} : `);
    }
  } else if (name === 'lib/cp.js') {
    const seam = 'export async function cp(src, dest, options) {';
    if (text.split(seam).length !== 2) {
      throw Error('Fs-copyfile cp seam drift');
    }
    // libuv clones files, not whole directories. Delegate to the real bounded
    // filesystem copier rather than claiming the native addon's directory clone.
    text = text.replace(seam, seam + `\n    if (${actual}) return nodeCp(src, dest, options);`);
  } else {
    throw Error('Unaudited fs-copyfile source');
  }
  return Buffer.from(text);
}

export function planFsCopyfile(root) {
  root = fs.realpathSync(root);
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, '@vscode/fs-copyfile'));
  if (!location.startsWith(modules + path.sep)) {
    throw Error('Fs-copyfile escapes prepared tree');
  }
  const metadata = fs.readFileSync(path.join(location, 'package.json'));
  const pkg = JSON.parse(metadata);
  if (
    pkg.name !== pin.name ||
    pkg.version !== pin.version ||
    hash(metadata) !== sourceHash('package.json')
  ) {
    throw Error('Fs-copyfile requires exact package2.0.0');
  }

  const relative = (name) =>
    path.relative(root, path.join(location, name)).split(path.sep).join('/');

  const changes = ['lib/native.js', 'lib/cp.js'].map((name) => ({
    path: relative(name),
    sourceSha256: sourceHash(name),
    bytes: transformFsCopyfile(fs.readFileSync(path.join(location, name)), name)
  }));
  const [primary, ...other] = changes;
  return {
    changes,
    record: {
      name: pin.name,
      version: pin.version,
      path: primary.path,
      sourceSha256: primary.sourceSha256,
      outputSha256: hash(primary.bytes),
      packageSha256: hash(metadata),
      builtin: 'mite:fs-copyfile',
      condition: 'actual Mite without Node/Electron markers',
      adaptation: 'real-volume-query-uv-file-copy-and-bounded-cp',
      additionalFiles: other.map((item) => ({
        path: item.path,
        sourceSha256: item.sourceSha256,
        outputSha256: hash(item.bytes)
      })),
      adapterSource: {
        path: 'tooling/code_oss/fs-copyfile.mjs',
        sha256: hash(fs.readFileSync(fileURLToPath(import.meta.url)))
      }
    }
  };
}

export function applyFsCopyfile(root, plan) {
  root = fs.realpathSync(root);
  const prepared = plan.changes.map((change) => {
    const target = fs.realpathSync(path.join(root, change.path));
    if (
      !target.startsWith(root + path.sep) ||
      hash(fs.readFileSync(target)) !== change.sourceSha256
    ) {
      throw Error('Copied fs-copyfile target drift');
    }
    return { target, bytes: change.bytes };
  });
  for (const { target, bytes } of prepared) {
    fs.writeFileSync(target, bytes);
  }
}

export function revertFsCopyfile(bytes, name) {
  if (!['lib/native.js', 'lib/cp.js'].includes(name)) {
    throw Error('Unaudited fs-copyfile source');
  }
  const original = fs.readFileSync(path.join(repository, 'vendor/fs-copyfile', name));
  if (!transformFsCopyfile(original, name).equals(bytes)) {
    throw Error('Adapted fs-copyfile source drift');
  }
  return original;
}
