/** Verified removal of only an exact previously audited undici adapter, before canonical --refresh. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash, randomUUID } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { verifiedRefresh } from './compile-refresh.mjs';
import { verifyPackageAdditions } from './package-adaptations.mjs';
import { transformUndici, transformUndiciFeatures } from './undici.mjs';

const repository = fileURLToPath(new URL('../../', import.meta.url));
const pin = JSON.parse(fs.readFileSync(path.join(repository, 'vendor/undici/manifest.json')));
const legacyAdapterSha256 = 'fbb6d56a91df533cb8c2e2d3a83aba66ec6ce33dbecdc22e263f02a75c41443a';
const featureAdapterSha256 = 'c717eeb2234ea908169f01051a98213dfd8829043e910bb927ae9df6f85fb10a';

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function prepareUndiciMigration(stage) {
  stage = fs.realpathSync(stage);
  const checked = verifiedRefresh(stage);
  const manifestFile = path.join(stage, 'ELYTRA-COMPILE.json');
  const manifestBytes = fs.readFileSync(manifestFile);
  const previous = checked.previous;
  const records = previous.packageAdaptations;
  if (
    !Array.isArray(records) ||
    records.filter((record) => record.name === 'undici').length !== 1
  ) {
    throw Error('Expected exactly one legacy undici adapter');
  }
  const original = fs.readFileSync(path.join(repository, 'vendor/undici/lib/core/util.js'));
  const target = fs.realpathSync(path.join(stage, 'node_modules/undici/lib/core/util.js'));
  const location = path.dirname(path.dirname(path.dirname(target)));

  const relative = (file) =>
    path.relative(stage, path.join(location, file)).split(path.sep).join('/');

  const old = records.find((record) => record.name === 'undici');
  const twoFiles = old.adapterSource?.sha256 === featureAdapterSha256;
  if (!twoFiles && old.adapterSource?.sha256 !== legacyAdapterSha256) {
    throw Error('Unknown prior undici adapter');
  }
  const featurePath = 'lib/util/runtime-features.js';
  const featureOriginal = fs.readFileSync(path.join(repository, 'vendor/undici', featurePath));
  const expected = {
    name: pin.name,
    version: pin.version,
    path: relative('lib/core/util.js'),
    sourceSha256: pin.sourceSha256,
    outputSha256: hash(transformUndici(original)),
    packageSha256: pin.packageSha256,
    adaptation: twoFiles
      ? 'undefined-unused-node-version-metadata-and-native-optional-feature-query'
      : 'undefined-unused-node-version-metadata',
    ...(twoFiles
      ? {
          additionalFiles: [
            {
              path: relative(featurePath),
              sourceSha256: hash(featureOriginal),
              outputSha256: hash(transformUndiciFeatures(featureOriginal))
            }
          ]
        }
      : {}),
    condition: 'actual Mite without Node/Electron version markers',
    auditFiles: pin.runtimeFiles
      .filter(
        (file) => file.path !== 'lib/core/util.js' && (!twoFiles || file.path !== featurePath)
      )
      .map((file) => ({ path: relative(file.path), sha256: file.sha256 })),
    adapterSource: {
      path: 'tooling/code_oss/undici.mjs',
      sha256: twoFiles ? featureAdapterSha256 : legacyAdapterSha256
    }
  };
  if (JSON.stringify(old) !== JSON.stringify(expected)) {
    throw Error('Legacy undici provenance differs from the exact audited adapter');
  }
  verifyPackageAdditions(stage, [old], [{ plan: { record: expected } }]);
  // A byte-local splice removes this one array entry. No other manifest value,
  // field order, whitespace or another package adapter is regenerated.
  const text = manifestBytes.toString('utf8');
  const serialized = JSON.stringify(old, null, 2)
    .split('\n')
    .map((line) => '    ' + line)
    .join('\n');
  const index = text.indexOf(serialized);
  if (index < 0 || text.indexOf(serialized, index + 1) !== -1) {
    throw Error('Legacy undici record serialization drift');
  }
  let start = index;
  let end = index + serialized.length;
  if (text.slice(end, end + 2) === ',\n') {
    end += 2;
  } else if (text.slice(start - 2, start) === ',\n') {
    start -= 2;
  }
  const next = Buffer.from(text.slice(0, start) + text.slice(end));
  const expectedManifest = {
    ...previous,
    packageAdaptations: records.filter((record) => record !== old)
  };
  if (JSON.stringify(JSON.parse(next)) !== JSON.stringify(expectedManifest)) {
    throw Error('Migration changed unrelated metadata');
  }
  return {
    stage,
    restores: [
      { target, original },
      ...(twoFiles
        ? [
            {
              target: fs.realpathSync(path.join(stage, relative(featurePath))),
              original: featureOriginal
            }
          ]
        : [])
    ],
    manifestFile,
    manifestBytes,
    next,
    sourceSha256: hash(fs.readFileSync(target)),
    manifestSha256: hash(manifestBytes),
    remainingAdapters: expectedManifest.packageAdaptations.length
  };
}

export function applyUndiciMigration(plan) {
  // Recheck every source/adapter hash immediately before the first mutation.
  const fresh = prepareUndiciMigration(plan.stage);
  if (fresh.sourceSha256 !== plan.sourceSha256 || fresh.manifestSha256 !== plan.manifestSha256) {
    throw Error('Stage changed after migration preflight');
  }
  plan = fresh;
  const suffix = '.undici-migration-' + randomUUID();
  const restores = plan.restores.map((item) => ({ ...item, temp: item.target + suffix }));
  const manifestTemp = plan.manifestFile + suffix;
  const backup = plan.manifestFile + suffix + '.previous';
  for (const item of restores) {
    fs.writeFileSync(item.temp, item.original, { flag: 'wx' });
  }
  fs.writeFileSync(manifestTemp, plan.next, { flag: 'wx' });
  // Once the completion manifest is absent, interrupted migrations fail closed.
  // Retain its exact original bytes for manual recovery/audit.
  fs.renameSync(plan.manifestFile, backup);
  for (const item of restores) {
    fs.renameSync(item.temp, item.target);
  }
  fs.renameSync(manifestTemp, plan.manifestFile);
  return backup;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  if (args.length < 1 || args.length > 2 || (args[1] !== undefined && args[1] !== '--apply')) {
    throw Error('Usage: migrate-undici.mjs <owned-stage> [--apply]');
  }
  const plan = prepareUndiciMigration(args[0]);
  const backup = args[1] === '--apply' ? applyUndiciMigration(plan) : undefined;
  console.log(
    JSON.stringify({
      operation: args[1] === '--apply' ? 'legacy-undici-removed' : 'legacy-undici-verified',
      remainingAdapters: plan.remainingAdapters,
      restoredFiles: plan.restores.length,
      backup
    })
  );
}
