/** Retire only the exact old scanner-diagnostics helper before ordinary verified refresh.
 * Default CLI operation is read-only. --apply removes the single audited output;
 * it never changes package adapters, other compiled files or generic refresh rules.
 */
import fs from 'node:fs';
import path from 'node:path';
import { createHash, randomUUID } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { verifiedRefresh } from './compile-refresh.mjs';
import { verifyPackageAdditions } from './package-adaptations.mjs';
import { verifyBuiltinExtensions } from './builtin-extensions-verify.mjs';

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const source = 'src/vs/platform/extensionManagement/common/elytraScannerDiagnostics.ts';
const output = 'src/vs/platform/extensionManagement/common/elytraScannerDiagnostics.js';
const sourceHash = '83502883d1a6a1b2fb3fc4b3a16af815176e0d9cf03ab60b830ca0f4545b7fa5';
const outputHash = '01675c1211ee5cf9fc84a1a78947bc9d84461d6d24aafac37a1f0b61d18ce04d';
const scanner = 'src/vs/platform/extensionManagement/common/extensionsScannerService.ts';
const adaptedHash = 'edebf0f1d4fc8c23f51b97df13c7fb733968509c476fc50e2a7192d8c6b7b760';
const packages = [
  '@vscode/deviceid',
  '@vscode/fs-copyfile',
  '@vscode/policy-watcher',
  '@vscode/sqlite3',
  'native-keymap',
  'undici'
].sort();

function verifyOverlay(root, expected) {
  root = path.resolve(root);
  if (fs.realpathSync(root) !== root || !fs.lstatSync(root).isDirectory()) {
    throw Error('Retirement requires a physical canonical overlay');
  }
  const manifest = path.join(root, 'ELYTRA-ADAPTATIONS.json');
  const bytes = fs.readFileSync(manifest);
  if (
    fs.realpathSync(manifest) !== manifest ||
    JSON.stringify(JSON.parse(bytes)) !== JSON.stringify(expected)
  ) {
    throw Error('Canonical overlay differs from stage provenance');
  }
  const files = new Map();
  for (const record of expected.files) {
    if (
      typeof record.path !== 'string' ||
      path.isAbsolute(record.path) ||
      record.path.split(/[\\/]/).some((part) => !part || part === '.' || part === '..') ||
      files.has(record.path)
    ) {
      throw Error('Invalid/duplicate overlay ownership');
    }
    const target = path.join(root, record.path);
    if (fs.realpathSync(target) !== target || !fs.lstatSync(target).isFile()) {
      throw Error('Overlay path escaped or changed type');
    }
    const data = fs.readFileSync(target);
    if (hash(data) !== record.overlaySha256) {
      throw Error('Edited canonical overlay');
    }
    files.set(record.path, data);
  }

  function walk(folder) {
    for (const entry of fs.readdirSync(folder, { withFileTypes: true })) {
      const target = path.join(folder, entry.name);
      if (entry.isSymbolicLink()) {
        throw Error('Overlay symlink');
      }
      if (entry.isDirectory()) {
        walk(target);
        continue;
      }
      if (!entry.isFile()) {
        throw Error('Overlay non-file');
      }
      if (entry.name === '.DS_Store' || target === manifest) {
        continue;
      }
      if (!files.has(path.relative(root, target).split(path.sep).join('/'))) {
        throw Error('Unknown canonical overlay file');
      }
    }
  }

  walk(root);
  return { root, manifest, bytes, files };
}

function removeExactElement(text, value) {
  const serialized = JSON.stringify(value, null, 2)
    .split('\n')
    .map((line) => '    ' + line)
    .join('\n');
  let index = text.indexOf(serialized);
  if (index < 0 || text.indexOf(serialized, index + 1) !== -1) {
    throw Error('Retired overlay element serialization differs');
  }
  let end = index + serialized.length;
  if (text.slice(end, end + 2) === ',\n') {
    end += 2;
  } else if (text.slice(index - 2, index) === ',\n') {
    index -= 2;
  } else {
    throw Error('Retired overlay element is not removable');
  }
  return text.slice(0, index) + text.slice(end);
}

export function prepareScannerDiagnosticsRetirement(stage, overlay) {
  stage = path.resolve(stage);
  const checked = verifiedRefresh(stage);
  const builtin = verifyBuiltinExtensions(stage);
  const manifest = path.join(stage, 'ELYTRA-COMPILE.json');
  const bytes = fs.readFileSync(manifest);
  if (hash(bytes) !== checked.manifestSha256) {
    throw Error('Stage changed during retirement verification');
  }
  const previous = checked.previous;
  const adapters = previous.packageAdaptations;
  if (
    !Array.isArray(adapters) ||
    JSON.stringify(adapters.map((item) => item.name).sort()) !== JSON.stringify(packages)
  ) {
    throw Error('Retirement requires the six known retained package adapters');
  }
  // This also checks each additional/audited package file and package manifest.
  verifyPackageAdditions(
    stage,
    adapters,
    adapters.map((record) => ({ plan: { record } }))
  );
  const matches = previous.inputs.filter(
    (record) => record.source === source || record.output === output
  );
  const expected = { source, sourceSha256: sourceHash, output, outputSha256: outputHash };
  if (matches.length !== 1 || JSON.stringify(matches[0]) !== JSON.stringify(expected)) {
    throw Error('Unknown retired scanner-diagnostics compiled input');
  }
  const originalScanner = previous.inputs.filter((record) => record.source === scanner);
  const expectedScanner = {
    source: scanner,
    sourceSha256: adaptedHash,
    output: scanner.replace(/\.ts$/, '.js'),
    outputSha256: 'f01e17726f2f41700fee473fa1f744b17ac13a8db77805295ded09a24e35fa9e'
  };
  if (
    originalScanner.length !== 1 ||
    JSON.stringify(originalScanner[0]) !== JSON.stringify(expectedScanner)
  ) {
    throw Error('Stage does not contain the exact retired scanner transformation');
  }
  const helpers = previous.adaptations?.files?.filter((record) => record.path === source);
  const expectedHelper = {
    path: source,
    ownerSource: 'firefly/compat/code-oss/scanner-diagnostics.ts',
    sourceSha256: sourceHash,
    overlaySha256: sourceHash
  };
  if (helpers?.length !== 1 || JSON.stringify(helpers[0]) !== JSON.stringify(expectedHelper)) {
    throw Error('Retired helper overlay provenance differs');
  }
  const oldScanner = previous.adaptations.files.filter((record) => record.path === scanner);
  if (
    oldScanner.length !== 1 ||
    oldScanner[0].overlaySha256 !== adaptedHash ||
    oldScanner[0].sourceSha256 !==
      '572182f1b3a3f10b2301d645bbaebfde27dbfc215d8e22b1f464f16339d78f48'
  ) {
    throw Error('Retired scanner overlay provenance differs');
  }
  const canonical = verifyOverlay(overlay, previous.adaptations);
  const diagnostic = previous.adaptations.adaptations.filter(
    (item) =>
      item ===
      'CA-29 opt-in bounded scalar extension-scanner await observations; no paths/payloads or semantic changes; remove after framework stall diagnosis'
  );
  if (diagnostic.length !== 1) {
    throw Error('Retired scanner diagnostic disclosure differs');
  }
  const nextOverlay = {
    ...previous.adaptations,
    files: previous.adaptations.files.filter(
      (record) => record.path !== source && record.path !== scanner
    ),
    adaptations: previous.adaptations.adaptations.filter((item) => item !== diagnostic[0])
  };
  let overlayText = canonical.bytes.toString('utf8');
  for (const item of [helpers[0], oldScanner[0], diagnostic[0]]) {
    overlayText = removeExactElement(overlayText, item);
  }
  const nextOverlayBytes = Buffer.from(overlayText);
  if (JSON.stringify(JSON.parse(nextOverlayBytes)) !== JSON.stringify(nextOverlay)) {
    throw Error('Retirement would change unrelated overlay metadata');
  }
  // Splice one exact array element, preserving every unrelated manifest byte.
  const serialized = JSON.stringify(expected, null, 2)
    .split('\n')
    .map((line) => '    ' + line)
    .join('\n');
  const text = bytes.toString('utf8');
  let index = text.indexOf(serialized);
  if (index < 0 || text.indexOf(serialized, index + 1) !== -1) {
    throw Error('Retired helper record serialization differs');
  }
  let end = index + serialized.length;
  if (text.slice(end, end + 2) === ',\n') {
    end += 2;
  } else if (text.slice(index - 2, index) === ',\n') {
    index -= 2;
  } else {
    throw Error('Retired helper is not a removable input array element');
  }
  const updated = Buffer.from(text.slice(0, index) + text.slice(end));
  const reduced = JSON.parse(updated);
  const expectedInputs = previous.inputs.filter((record) => record.output !== output);
  if (JSON.stringify(reduced) !== JSON.stringify({ ...previous, inputs: expectedInputs })) {
    throw Error('Retirement would change unrelated stage metadata');
  }
  return {
    stage,
    summary: {
      output,
      overlay: canonical.root,
      retiredOverlayFiles: [source, scanner],
      originalScannerSha256: oldScanner[0].sourceSha256,
      sourceSha256: sourceHash,
      outputSha256: outputHash,
      retainedPackages: packages,
      previousManifestSha256: hash(bytes)
    },

    apply() {
      const lock = path.join(stage, '.ELYTRA-SCANNER-RETIREMENT.lock');
      fs.mkdirSync(lock); // Never steal an earlier/concurrent transaction.
      try {
        const again = verifiedRefresh(stage);
        verifyPackageAdditions(
          stage,
          adapters,
          adapters.map((record) => ({ plan: { record } }))
        );
        const currentOverlay = verifyOverlay(canonical.root, previous.adaptations);
        if (
          again.manifestSha256 !== hash(bytes) ||
          !fs.readFileSync(manifest).equals(bytes) ||
          !currentOverlay.bytes.equals(canonical.bytes)
        ) {
          throw Error('Stage/overlay changed after retirement planning');
        }
        const token = randomUUID();
        const backup = `ELYTRA-SCANNER-RETIREMENT-${token}`;
        const directory = path.join(stage, backup);
        fs.mkdirSync(directory, { mode: 0o700 });

        const save = (name, data) =>
          fs.writeFileSync(path.join(directory, name), data, { flag: 'wx', mode: 0o600 });

        save('ELYTRA-COMPILE.before.json', bytes);
        save('ELYTRA-ADAPTATIONS.before.json', canonical.bytes);
        save('scanner.before.ts', canonical.files.get(scanner));
        save('helper.before.ts', canonical.files.get(source));
        save('helper.before.js', fs.readFileSync(path.join(stage, output)));
        const journal = path.join(backup, 'retirement.json');
        save(
          'retirement.json',
          JSON.stringify(
            {
              schema: 1,
              kind: 'retired-scanner-diagnostic-awaiting-canonical-refresh',
              ...this.summary,
              intermediateManifestSha256: hash(updated),
              nextOverlaySha256: hash(nextOverlayBytes),
              retainedPackageAdaptations: adapters,
              retainedBuiltinInventory: builtin
            },
            null,
            2
          ) + '\n'
        );
        // Both completion claims become unavailable before deleting owned data.
        // Interrupted transactions remain fail-closed and retain exact backups.
        fs.unlinkSync(manifest);
        fs.unlinkSync(canonical.manifest);
        fs.unlinkSync(path.join(stage, output));
        fs.unlinkSync(path.join(canonical.root, source));
        fs.unlinkSync(path.join(canonical.root, scanner));
        const overlayTemporary = path.join(
          canonical.root,
          `ELYTRA-ADAPTATIONS.retiring-${token}.json`
        );
        fs.writeFileSync(overlayTemporary, nextOverlayBytes, { flag: 'wx', mode: 0o600 });
        fs.renameSync(overlayTemporary, canonical.manifest);
        const temporary = path.join(stage, `ELYTRA-COMPILE.retiring-${token}.json`);
        fs.writeFileSync(temporary, updated, { flag: 'wx', mode: 0o600 });
        fs.renameSync(temporary, manifest);
        verifiedRefresh(stage);
        verifyOverlay(canonical.root, nextOverlay);
        return { backup, journal, ...this.summary };
      } finally {
        fs.rmdirSync(lock);
      }
    }
  };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  if (args.length < 2 || args.length > 3 || (args.length === 3 && args[2] !== '--apply')) {
    throw Error('Usage: retire-scanner-diagnostics.mjs STAGE OVERLAY [--apply]');
  }
  const retirement = prepareScannerDiagnosticsRetirement(args[0], args[1]);
  console.log(JSON.stringify(args[2] === '--apply' ? retirement.apply() : retirement.summary));
}
