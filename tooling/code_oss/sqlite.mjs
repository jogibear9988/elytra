/** Audited opt-in adaptation of exactly @vscode/sqlite3 5.1.12-vscode in a fresh compiled tree. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const sourceSha256 = '8d6b400ae7f69a80d0cdd37a968d7b9a913661fa53475e5b8de49dda21684973';
const packageSha256 = '5242b530769028cafcec857815ef783a4b310ce157572231730498057895a008';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function transformSQLite(bytes) {
  if (digest(bytes) !== sourceSha256) {
    throw Error('SQLite 5.1.12-vscode entry drift');
  }
  // Ordinary Node/Electron executes every original package byte unchanged.
  // Only Mite selects its explicit real SQLite service, never the N-API loader.
  return Buffer.from(
    "if (typeof process.versions.mite === 'string') {\n" +
      "  module.exports = require('mite:sqlite');\n} else {\n" +
      bytes.toString('utf8') +
      '\n}\n'
  );
}

export function planSQLite(root) {
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, '@vscode/sqlite3'));
  const relative = path.relative(modules, location);
  if (relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
    throw Error('SQLite package escapes prepared dependency tree');
  }
  const packageBytes = fs.readFileSync(path.join(location, 'package.json'));
  const manifest = JSON.parse(packageBytes);
  if (
    manifest.name !== '@vscode/sqlite3' ||
    manifest.version !== '5.1.12-vscode' ||
    manifest.main !== './lib/sqlite3' ||
    digest(packageBytes) !== packageSha256
  ) {
    throw Error('SQLite adaptation requires exact audited package 5.1.12-vscode');
  }
  const source = fs.readFileSync(path.join(location, 'lib/sqlite3.js'));
  const bytes = transformSQLite(source);
  const filename = fileURLToPath(import.meta.url);
  const repository = fileURLToPath(new URL('../../', import.meta.url));
  const record = {
    name: manifest.name,
    version: manifest.version,
    path: path.relative(root, path.join(location, 'lib/sqlite3.js')).split(path.sep).join('/'),
    sourceSha256,
    outputSha256: digest(bytes),
    packageSha256,
    builtin: 'mite:sqlite',
    condition: "typeof process.versions.mite === 'string'",
    adapterSource: {
      path: path.relative(repository, filename).split(path.sep).join('/'),
      sha256: digest(fs.readFileSync(filename))
    }
  };
  return { record, bytes };
}

export function applySQLite(root, plan) {
  const target = path.join(root, plan.record.path);
  const resolved = fs.realpathSync(target);
  const relative = path.relative(fs.realpathSync(root), resolved);
  if (
    relative === '..' ||
    relative.startsWith('..' + path.sep) ||
    path.isAbsolute(relative) ||
    digest(fs.readFileSync(resolved)) !== plan.record.sourceSha256
  ) {
    throw Error('Copied SQLite target escaped or changed before adaptation');
  }
  fs.writeFileSync(resolved, plan.bytes);
}
