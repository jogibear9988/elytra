/** Retire only the exact old diff-worker helper before ordinary verified refresh.
 * Default CLI operation is read-only. --apply removes the single audited output;
 * it never changes package adapters, other compiled files or generic refresh rules.
 */
import fs from 'node:fs';
import path from 'node:path';
import { createHash, randomUUID } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { verifiedRefresh } from './compile-refresh.mjs';
import { verifyPackageAdditions } from './package-adaptations.mjs';

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const source = 'src/vs/base/common/elytraDiffWorker.ts';
const output = 'src/vs/base/common/elytraDiffWorker.js';
const sourceHash = 'c7062426dc4b286a0f49975c35c3d1682b8d6347514034b747f8ab0d636cf6bf';
const outputHash = 'ec3c7d0c4867646e4e94653cd10bf44aed893a9a382556bdfcc35529ca238cab';
const diff = 'src/vs/platform/agentHost/node/diffComputeService.ts';
const adaptedHash = 'e1073d3dfb6289e0c3b4851a130040f49f5d2454e355d47e2719a27cae6bf1a7';
const packages = [
  '@vscode/deviceid',
  '@vscode/fs-copyfile',
  '@vscode/policy-watcher',
  '@vscode/sqlite3',
  'native-keymap',
  'undici'
].sort();

export function prepareDiffWorkerRetirement(stage) {
  stage = path.resolve(stage);
  const checked = verifiedRefresh(stage);
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
    throw Error('Unknown retired diff-worker compiled input');
  }
  const originalDiff = previous.inputs.filter((record) => record.source === diff);
  if (originalDiff.length !== 1 || originalDiff[0].sourceSha256 !== adaptedHash) {
    throw Error('Stage does not contain the exact retired diff-service transformation');
  }
  const helpers = previous.adaptations?.files?.filter((record) => record.path === source);
  const expectedHelper = {
    path: source,
    ownerSource: 'firefly/compat/code-oss/diff-worker.ts',
    sourceSha256: sourceHash,
    overlaySha256: sourceHash
  };
  if (helpers?.length !== 1 || JSON.stringify(helpers[0]) !== JSON.stringify(expectedHelper)) {
    throw Error('Retired helper overlay provenance differs');
  }
  const oldDiff = previous.adaptations.files.filter((record) => record.path === diff);
  if (
    oldDiff.length !== 1 ||
    oldDiff[0].overlaySha256 !== adaptedHash ||
    oldDiff[0].sourceSha256 !== 'd33579abe20a405fdc4737d3e7ef2367eadbc41b7d5854b5fe30f77a78bf74de'
  ) {
    throw Error('Retired diff-service overlay provenance differs');
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
      sourceSha256: sourceHash,
      outputSha256: outputHash,
      retainedPackages: packages,
      previousManifestSha256: hash(bytes)
    },

    apply() {
      // Recheck immediately before the first mutation. Failure leaves all
      // completion metadata and compiled outputs untouched.
      const again = verifiedRefresh(stage);
      verifyPackageAdditions(
        stage,
        adapters,
        adapters.map((record) => ({ plan: { record } }))
      );
      if (again.manifestSha256 !== hash(bytes) || !fs.readFileSync(manifest).equals(bytes)) {
        throw Error('Stage changed after retirement planning');
      }
      const token = randomUUID();
      const backup = `ELYTRA-COMPILE.before-diff-retirement-${token}.json`;
      const journal = `ELYTRA-DIFF-RETIREMENT-${token}.json`;
      fs.copyFileSync(manifest, path.join(stage, backup), fs.constants.COPYFILE_EXCL);
      fs.writeFileSync(
        path.join(stage, journal),
        JSON.stringify(
          {
            schema: 1,
            kind: 'retired-owned-output-awaiting-canonical-refresh',
            backup,
            ...this.summary,
            intermediateManifestSha256: hash(updated)
          },
          null,
          2
        ) + '\n',
        { flag: 'wx', mode: 0o600 }
      );
      // An interrupted operation must not leave the old completed manifest
      // claiming that its now-retired helper still exists.
      fs.unlinkSync(manifest);
      fs.unlinkSync(path.join(stage, output));
      const temporary = path.join(stage, `ELYTRA-COMPILE.retiring-${token}.json`);
      fs.writeFileSync(temporary, updated, { flag: 'wx', mode: 0o600 });
      fs.renameSync(temporary, manifest);
      verifiedRefresh(stage);
      return { backup, journal, ...this.summary };
    }
  };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  if (args.length < 1 || args.length > 2 || (args.length === 2 && args[1] !== '--apply')) {
    throw Error('Usage: retire-diff-worker.mjs STAGE [--apply]');
  }
  const retirement = prepareDiffWorkerRetirement(args[0]);
  console.log(JSON.stringify(args[1] === '--apply' ? retirement.apply() : retirement.summary));
}
