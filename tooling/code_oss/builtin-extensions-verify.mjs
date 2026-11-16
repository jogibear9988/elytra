/** Optional built-in resource ownership, independent of the compiler's output set.
 * Synchronous and read-only so verifiedRefresh can compose this check without
 * weakening its existing orphan/collision rules. A seal detects metadata drift;
 * it is not a signature. Static resources also match the maintained source pin.
 */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const pin = JSON.parse(fs.readFileSync(new URL('./builtin-extensions-pin.json', import.meta.url)));
const name = 'ELYTRA-BUILTIN-EXTENSIONS.json';

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const maxBytes = 1024 * 1024 * 1024;

export function canonicalBuiltinInventory(value) {
  if (Array.isArray(value)) {
    return '[' + value.map(canonicalBuiltinInventory).join(',') + ']';
  }
  if (value !== null && typeof value === 'object') {
    return (
      '{' +
      Object.keys(value)
        .sort()
        .map((key) => JSON.stringify(key) + ':' + canonicalBuiltinInventory(value[key]))
        .join(',') +
      '}'
    );
  }
  return JSON.stringify(value);
}

function require(condition, message) {
  if (!condition) {
    throw Error('Built-in extension inventory: ' + message);
  }
}

function relative(value) {
  require(typeof value === 'string' &&
    value.length > 0 &&
    value.length <= 4096 &&
    !value.includes('\\') &&
    !path.isAbsolute(value), 'invalid resource path');
  require(!value.includes('\0') &&
    value
      .split('/')
      .every((part) => part && part !== '.' && part !== '..'), 'unsafe resource path');
  require(value.startsWith('extensions/') ||
    value.startsWith('.build/builtInExtensions/'), 'resource escaped owned roots');
}

function physical(root, target, optional = false) {
  let current = target;
  while (current !== path.dirname(root)) {
    const info = fs.lstatSync(current, { throwIfNoEntry: false });
    require(!info?.isSymbolicLink(), 'symlink in resource path');
    if (current === target && !info && !optional) {
      throw Error('Missing built-in resource: ' + path.relative(root, target));
    }
    current = path.dirname(current);
  }
  return fs.lstatSync(target, { throwIfNoEntry: false });
}

function readJSON(root, target) {
  const info = physical(root, target);
  require(info.isFile() && info.size <= 32 * 1024 * 1024, 'invalid metadata file');
  return JSON.parse(fs.readFileSync(target, 'utf8'));
}

export function verifyBuiltinExtensions(input) {
  const stage = path.resolve(input);
  require(fs.realpathSync(stage) === stage &&
    fs.lstatSync(stage).isDirectory(), 'stage is not canonical');
  for (const pending of ['.elytra-builtin-static.pending', name + '.pending']) {
    require(!physical(stage, path.join(stage, pending), true), 'incomplete resource publication');
  }
  const marker = path.join(stage, name);
  const roots = [path.join(stage, 'extensions'), path.join(stage, '.build/builtInExtensions')];
  const markerInfo = physical(stage, marker, true);
  if (!markerInfo) {
    require(roots.every(
      (root) => !physical(stage, root, true)
    ), 'unowned built-in extension directory');
    return null;
  }
  const report = readJSON(stage, marker);
  require(report.schema === 1 &&
    report.kind === 'owned-built-in-extension-stage' &&
    report.revision === pin.revision &&
    report.archiveSha256 === pin.archiveSha256, 'source identity differs');
  require(report.mode === 'partial-static-only' ||
    report.mode === 'upstream-standard-build', 'unknown staging mode');
  require(report.runtimeAcceptance === 'not-established', 'unverified runtime acceptance claim');
  require(report.seal?.algorithm === 'sha256' &&
    /^[a-f0-9]{64}$/.test(report.seal.digest), 'missing metadata seal');
  const { seal, ...body } = report;
  require(hash(Buffer.from(canonicalBuiltinInventory(body))) ===
    seal.digest, 'metadata seal differs');
  const compiled = readJSON(stage, path.join(stage, 'ELYTRA-COMPILE.json'));
  require(compiled.schema === 1 &&
    compiled.kind === 'diagnostic-source-tree' &&
    compiled.adaptations?.revision === pin.revision, 'compiler source identity differs');
  require(Array.isArray(report.files) &&
    report.files.length > 0 &&
    report.files.length <= 100000, 'invalid file count');
  const expected = new Map();
  let bytes = 0;
  for (const item of report.files) {
    relative(item.path);
    require(!expected.has(item.path), 'duplicate file record');
    require(Number.isSafeInteger(item.bytes) &&
      item.bytes >= 0 &&
      item.bytes <= 64 * 1024 * 1024 &&
      /^[a-f0-9]{64}$/.test(item.sha256) &&
      typeof item.executable === 'boolean', 'invalid file record');
    bytes += item.bytes;
    require(bytes <= maxBytes, 'file byte budget exceeded');
    expected.set(item.path, item);
  }
  if (report.mode === 'partial-static-only') {
    require(expected.size === pin.staticFiles.length &&
      bytes === pin.staticBytes, 'partial static inventory incomplete');
    for (const original of pin.staticFiles) {
      require(canonicalBuiltinInventory(expected.get(original.path)) ===
        canonicalBuiltinInventory(original), 'static resource differs from maintained source pin');
    }
    require(Array.isArray(report.stagedExtensions) &&
      report.stagedExtensions.length === pin.staticCount, 'static extension count differs');
    require(Array.isArray(report.notStaged) &&
      canonicalBuiltinInventory(report.notStaged) ===
        canonicalBuiltinInventory(
          pin.needsBuildExtensions
        ), 'pending runtime extension disclosure differs');
    require(report.includeCopilot === false &&
      canonicalBuiltinInventory(report.extensions) ===
        canonicalBuiltinInventory(
          [...pin.staticExtensions, ...pin.needsBuildExtensions].sort((a, b) =>
            a.directory < b.directory ? -1 : a.directory > b.directory ? 1 : 0
          )
        ), 'source classification differs');
    require(Array.isArray(report.marketplaceStatus) &&
      report.marketplaceStatus.length === 3 &&
      report.marketplaceStatus.every(
        (item) => item.staged === false && item.archiveVerification === 'not-downloaded'
      ), 'partial stage claims downloaded extensions');
  }
  let seen = 0;

  function walk(directory) {
    const info = physical(stage, directory, true);
    if (!info) {
      return;
    }
    require(info.isDirectory(), 'owned root changed type');
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      const target = path.join(directory, entry.name);
      require(!entry.isSymbolicLink(), 'symlink in published resource tree');
      if (entry.isDirectory()) {
        const prefix = path.relative(stage, target).split(path.sep).join('/') + '/';
        require([...expected.keys()].some((name) =>
          name.startsWith(prefix)
        ), 'unknown published directory');
        walk(target);
        continue;
      }
      require(entry.isFile(), 'published resource is not regular file');
      if (entry.name === '.DS_Store') {
        continue;
      }
      const relativeName = path.relative(stage, target).split(path.sep).join('/');
      const item = expected.get(relativeName);
      require(item !== undefined, 'unknown published resource: ' + relativeName);
      const stat = fs.statSync(target);
      require(stat.size === item.bytes &&
        hash(fs.readFileSync(target)) === item.sha256, 'resource content differs: ' + relativeName);
      if (process.platform !== 'win32') {
        require(Boolean(stat.mode & 0o111) === item.executable, 'resource executable flag differs');
      }
      ++seen;
    }
  }

  for (const root of roots) {
    walk(root);
  }
  require(seen === expected.size, 'missing published resource');
  const locations = new Set();
  for (const item of report.stagedExtensions ?? []) {
    require(!locations.has(item.location), 'duplicate extension identity');
    locations.add(item.location);
    require(item.activation === 'unverified', 'unverified extension activation claim');
    relative(item.location + '/package.json');
    const manifestRecord = expected.get(item.location + '/package.json');
    require(manifestRecord?.sha256 === item.manifestSha256, 'manifest record differs');
    const manifest = readJSON(stage, path.join(stage, item.location, 'package.json'));
    require(item.id === manifest.publisher + '.' + manifest.name &&
      item.version === manifest.version, 'extension identity differs');
    require(item.main === (manifest.main ?? null) &&
      item.browser === (manifest.browser ?? null), 'extension entry metadata differs');
  }
  if (report.mode === 'partial-static-only') {
    require(pin.staticExtensions.every((item) =>
      locations.has('extensions/' + item.directory)
    ), 'static extension location differs');
    require(canonicalBuiltinInventory(report.marketplace) ===
      canonicalBuiltinInventory(pin.productVSIX), 'product source pins differ');
  }
  return {
    mode: report.mode,
    extensionCount: report.stagedExtensions?.length ?? 0,
    files: seen,
    bytes,
    seal: seal.digest
  };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  if (process.argv.length !== 3) {
    throw Error('Usage: builtin-extensions-verify.mjs STAGE');
  }
  console.log(JSON.stringify(verifyBuiltinExtensions(process.argv[2])));
}
