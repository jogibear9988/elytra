/** Exact native-keymap 3.3.9 loader adaptation; ordinary Electron remains unchanged. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const sourceSha256 = '4e5aaf2b1661850fb01e90a72720c99ff8976c9c9304ee7cd84755ac43ed9299';
const packageSha256 = 'bee751b38ff1035305c82449f92d309ee5315bea42091ab3bf54f5223253bed1';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function transformNativeKeymap(bytes) {
  if (digest(bytes) !== sourceSha256) {
    throw Error('Native keymap 3.3.9 entry drift');
  }
  // Keep the entire audited Node/Electron implementation byte-for-byte. The
  // Mite branch replaces lazy N-API wrappers with direct genuine OS exports,
  // so unsupported native queries throw instead of being swallowed as null/[] .
  const source = bytes.toString('utf8');
  return Buffer.from(
    source +
      "\nif (typeof process.versions.mite === 'string') {\n" +
      "  const native = require('mite:native-keymap');\n" +
      '  exports.getCurrentKeyboardLayout = native.getCurrentKeyboardLayout;\n' +
      '  exports.getKeyMap = native.getKeyMap;\n' +
      '  exports.onDidChangeKeyboardLayout = native.onDidChangeKeyboardLayout;\n' +
      '  exports.isISOKeyboard = native.isISOKeyboard;\n' +
      '}\n'
  );
}

export function planNativeKeymap(root) {
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, 'native-keymap'));
  const relative = path.relative(modules, location);
  if (relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
    throw Error('Native keymap package escapes prepared dependency tree');
  }
  const packageBytes = fs.readFileSync(path.join(location, 'package.json'));
  const manifest = JSON.parse(packageBytes);
  if (
    manifest.name !== 'native-keymap' ||
    manifest.version !== '3.3.9' ||
    manifest.main !== 'index.js' ||
    digest(packageBytes) !== packageSha256
  ) {
    throw Error('Native keymap adaptation requires exact audited package 3.3.9');
  }
  const source = fs.readFileSync(path.join(location, 'index.js'));
  const bytes = transformNativeKeymap(source);
  const filename = fileURLToPath(import.meta.url);
  const repository = fileURLToPath(new URL('../../', import.meta.url));
  const record = {
    name: manifest.name,
    version: manifest.version,
    path: path.relative(root, path.join(location, 'index.js')).split(path.sep).join('/'),
    sourceSha256,
    outputSha256: digest(bytes),
    packageSha256,
    builtin: 'mite:native-keymap',
    condition: "typeof process.versions.mite === 'string'",
    adapterSource: {
      path: path.relative(repository, filename).split(path.sep).join('/'),
      sha256: digest(fs.readFileSync(filename))
    }
  };
  return { record, bytes };
}

export function applyNativeKeymap(root, plan) {
  const target = path.join(root, plan.record.path);
  const resolved = fs.realpathSync(target);
  const relative = path.relative(fs.realpathSync(root), resolved);
  if (
    relative === '..' ||
    relative.startsWith('..' + path.sep) ||
    path.isAbsolute(relative) ||
    digest(fs.readFileSync(resolved)) !== plan.record.sourceSha256
  ) {
    throw Error('Copied native keymap target escaped or changed before adaptation');
  }
  fs.writeFileSync(resolved, plan.bytes);
}
