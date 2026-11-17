/** Select a real alternate-runtime PTY backend at the exact pinned package entry.
 * Preserve the complete original Node/Electron branch and all license notices.
 */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const sourceSha256 = '17e79806518ab350cd7d729dcc94154c5d770ea03e38e53684c5a8618065f99c';
const packageSha256 = '426bf5fa25d9c3b86fa8db19d1043f12ea40635dd44fa6137554585b28e6bbbc';
const prefix =
  '"use strict";\nif (typeof process.versions.mite === "string") {\n' +
  '  module.exports = require("mite:pty");\n} else {\n';
const suffix = '\n}\n';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function transformNodePty(bytes) {
  if (digest(bytes) !== sourceSha256) {
    throw Error('Node-pty 1.2.0-beta.18 entry drift');
  }
  const transformed = Buffer.concat([Buffer.from(prefix), bytes, Buffer.from(suffix)]);
  if (!transformed.subarray(Buffer.byteLength(prefix), -Buffer.byteLength(suffix)).equals(bytes)) {
    throw Error('Node-pty source preservation failed');
  }
  return transformed;
}

export function planNodePty(root) {
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, 'node-pty'));
  const relative = path.relative(modules, location);
  if (relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
    throw Error('Node-pty escapes the prepared dependency tree');
  }
  const packageBytes = fs.readFileSync(path.join(location, 'package.json'));
  const manifest = JSON.parse(packageBytes);
  if (
    manifest.name !== 'node-pty' ||
    manifest.version !== '1.2.0-beta.18' ||
    manifest.main !== './lib/index.js' ||
    digest(packageBytes) !== packageSha256
  ) {
    throw Error('Node-pty adaptation requires the exact audited package');
  }
  const bytes = transformNodePty(fs.readFileSync(path.join(location, 'lib/index.js')));
  const filename = fileURLToPath(import.meta.url);
  const repository = fileURLToPath(new URL('../../', import.meta.url));
  return {
    bytes,
    record: {
      name: manifest.name,
      version: manifest.version,
      path: path.relative(root, path.join(location, 'lib/index.js')).split(path.sep).join('/'),
      sourceSha256,
      outputSha256: digest(bytes),
      packageSha256,
      builtin: 'mite:pty',
      condition: 'typeof process.versions.mite === "string"',
      adapterSource: {
        path: path.relative(repository, filename).split(path.sep).join('/'),
        sha256: digest(fs.readFileSync(filename))
      }
    }
  };
}

export function applyNodePty(root, plan) {
  const target = path.join(root, plan.record.path);
  const resolved = fs.realpathSync(target);
  const relative = path.relative(fs.realpathSync(root), resolved);
  if (
    relative === '..' ||
    relative.startsWith('..' + path.sep) ||
    path.isAbsolute(relative) ||
    digest(fs.readFileSync(resolved)) !== plan.record.sourceSha256
  ) {
    throw Error('Copied node-pty target escaped or changed before adaptation');
  }
  fs.writeFileSync(resolved, plan.bytes);
}
