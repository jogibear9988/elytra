/** Audited opt-in adaptation of exactly @vscode/policy-watcher 1.7.0 in a fresh compiled tree. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const sourceSha256 = '4d21c1fd7adefabd5bc9ce1744ccd0d898165ec02886b492cf4accd2bb4b98ca';
const packageSha256 = '5d04fc9f1c5f77bae9e3cc982048357be6bc19132cebc7aae0691907d11249a6';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function transformPolicyWatcher(bytes) {
  if (digest(bytes) !== sourceSha256) {
    throw Error('Policy watcher 1.7.0 entry drift');
  }
  const original = "exports.createWatcher = require('bindings')('vscode-policy-watcher');";
  const source = bytes.toString('utf8');
  if (source.split(original).length !== 2) {
    throw Error('Unexpected policy watcher native loader seam');
  }
  return Buffer.from(
    source.replace(
      original,
      "exports.createWatcher = typeof process.versions.mite === 'string'\n" +
        "  ? require('mite:policy-watcher').createWatcher\n" +
        "  : require('bindings')('vscode-policy-watcher');"
    )
  );
}

export function planPolicyWatcher(root) {
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, '@vscode/policy-watcher'));
  const relative = path.relative(modules, location);
  if (relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
    throw Error('Policy watcher package escapes prepared dependency tree');
  }
  const packageBytes = fs.readFileSync(path.join(location, 'package.json'));
  const manifest = JSON.parse(packageBytes);
  if (
    manifest.name !== '@vscode/policy-watcher' ||
    manifest.version !== '1.7.0' ||
    manifest.main !== 'index.js' ||
    digest(packageBytes) !== packageSha256
  ) {
    throw Error('Policy watcher adaptation requires exact audited package 1.7.0');
  }
  const source = fs.readFileSync(path.join(location, 'index.js'));
  const bytes = transformPolicyWatcher(source);
  const filename = fileURLToPath(import.meta.url);
  const repository = fileURLToPath(new URL('../../', import.meta.url));
  const record = {
    name: manifest.name,
    version: manifest.version,
    path: path.relative(root, path.join(location, 'index.js')).split(path.sep).join('/'),
    sourceSha256,
    outputSha256: digest(bytes),
    packageSha256,
    builtin: 'mite:policy-watcher',
    condition: "typeof process.versions.mite === 'string'",
    adapterSource: {
      path: path.relative(repository, filename).split(path.sep).join('/'),
      sha256: digest(fs.readFileSync(filename))
    }
  };
  return { record, bytes };
}

export function applyPolicyWatcher(root, plan) {
  const target = path.join(root, plan.record.path);
  const resolved = fs.realpathSync(target);
  const relative = path.relative(fs.realpathSync(root), resolved);
  if (
    relative === '..' ||
    relative.startsWith('..' + path.sep) ||
    path.isAbsolute(relative) ||
    digest(fs.readFileSync(resolved)) !== plan.record.sourceSha256
  ) {
    throw Error('Copied policy watcher target escaped or changed before adaptation');
  }
  fs.writeFileSync(resolved, plan.bytes);
}
