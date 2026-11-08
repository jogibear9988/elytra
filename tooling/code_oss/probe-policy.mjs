/** Verify unchanged consumers plus the explicit package adapter, then run genuine native policy reads. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash, randomUUID } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import { transformPolicyWatcher } from './policy-watcher.mjs';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, miteArg] = process.argv.slice(2);
if (!treeArg || !miteArg) {
  throw Error(
    'Require compiled Code-OSS tree with explicit policy adapter and real Mite executable'
  );
}
if (process.platform !== 'darwin') {
  throw Error('Genuine native policy consumer acceptance is currently macOS only');
}
const tree = fs.realpathSync(path.resolve(treeArg));
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
const outputs = [
  'src/vs/platform/policy/node/nativePolicyService.js',
  'src/vs/platform/policy/node/nativeManagedSettingsService.js'
];
if (report.kind !== 'diagnostic-source-tree' || report.adaptations?.revision !== pin.revision) {
  throw Error('Pinned Code-OSS native policy source/compiled revision drift');
}
for (const output of outputs) {
  const name = path.posix.basename(output, '.js') + '.ts';
  const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests', name));
  if (
    hash(source) !== pin.files[name]?.sha256 ||
    records.get(output)?.sourceSha256 !== hash(source)
  ) {
    throw Error('Pinned Code-OSS native policy source/compiled input drift: ' + name);
  }
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  if (file.startsWith('../') || path.posix.isAbsolute(file)) {
    throw Error('Compiled policy dependency escaped tree: ' + file);
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled policy dependency drift: ' + file);
  }
  verified.add(file);
  for (const match of bytes
    .toString()
    .matchAll(/(?:\bfrom\s*|\bimport\s*(?:\(\s*)?)["']([^"']+)["']/g)) {
    if (match[1].startsWith('.')) {
      verify(path.posix.normalize(path.posix.join(path.posix.dirname(file), match[1])));
    }
  }
}

for (const output of outputs) {
  verify(output);
}
verify('src/nls.messages.js');

// Independently checked against the official 1.7.0 npm archive, not inferred
// from a successful import. The owned adapter must disclose its source too.
const sourceIndexHash = '4d21c1fd7adefabd5bc9ce1744ccd0d898165ec02886b492cf4accd2bb4b98ca';
const sourcePackageHash = '5d04fc9f1c5f77bae9e3cc982048357be6bc19132cebc7aae0691907d11249a6';
const adaptations = (report.packageAdaptations ?? []).filter(
  (item) => item.name === '@vscode/policy-watcher'
);
if (adaptations.length !== 1) {
  throw Error('Require exactly one explicit @vscode/policy-watcher adaptation');
}
const adapter = adaptations[0];
if (
  adapter.version !== '1.7.0' ||
  adapter.sourceSha256 !== sourceIndexHash ||
  adapter.packageSha256 !== sourcePackageHash ||
  adapter.builtin !== 'mite:policy-watcher' ||
  adapter.condition !== "typeof process.versions.mite === 'string'"
) {
  throw Error('Policy adapter source/version/builtin provenance drift');
}
const entry = fs.realpathSync(path.resolve(tree, adapter.path));
const relative = path.relative(tree, entry);
if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !relative) {
  throw Error('Policy adapter entry escaped compiled dependency tree');
}
const adaptedBytes = fs.readFileSync(entry);
if (hash(adaptedBytes) !== adapter.outputSha256) {
  throw Error('Policy adapter output drift');
}
// Reconstruct the exact original seam and independently replay the owned
// transform. Self-reported output hashes alone must not authorize another loader.
const adaptedSeam =
  "exports.createWatcher = typeof process.versions.mite === 'string'\n" +
  "  ? require('mite:policy-watcher').createWatcher\n" +
  "  : require('bindings')('vscode-policy-watcher');";
const originalSeam = "exports.createWatcher = require('bindings')('vscode-policy-watcher');";
const adaptedText = adaptedBytes.toString('utf8');
if (
  adaptedText.split(adaptedSeam).length !== 2 ||
  !transformPolicyWatcher(Buffer.from(adaptedText.replace(adaptedSeam, originalSeam))).equals(
    adaptedBytes
  )
) {
  throw Error('Policy adapter is not the exact audited conditional native-loader transform');
}
const packageJSON = fs.readFileSync(path.join(path.dirname(entry), 'package.json'));
if (hash(packageJSON) !== sourcePackageHash || JSON.parse(packageJSON).version !== '1.7.0') {
  throw Error('Adapted policy package metadata drift');
}
// Exact source metadata fields are maintained by compile.mjs's opt-in adapter.
const adapterSource = adapter.adapterSource;
if (
  !adapterSource ||
  adapterSource.path !== 'tooling/code_oss/policy-watcher.mjs' ||
  typeof adapterSource.sha256 !== 'string'
) {
  throw Error('Policy adapter lacks owned source provenance');
}
const owned = path.resolve(root, adapterSource.path);
const ownedRelative = path.relative(root, owned);
if (
  ownedRelative.startsWith(`..${path.sep}`) ||
  path.isAbsolute(ownedRelative) ||
  hash(fs.readFileSync(owned)) !== adapterSource.sha256
) {
  throw Error('Owned policy adapter source drift');
}

const domain = 'com.elytra.policy.probe.' + randomUUID().replaceAll('-', '');
const result = spawnSync(
  path.resolve(miteArg),
  [path.join(root, 'mite/tests/policy-watcher/consumer-entry.mjs')],
  {
    encoding: 'utf8',
    timeout: 20000,
    env: {
      ...process.env,
      ELYTRA_POLICY_DOMAIN: domain,
      ELYTRA_POLICY_CONSUMER: pathToFileURL(path.join(tree, outputs[0])).href,
      ELYTRA_MANAGED_POLICY_CONSUMER: pathToFileURL(path.join(tree, outputs[1])).href,
      ELYTRA_POLICY_NLS: pathToFileURL(path.join(tree, 'src/nls.messages.js')).href
    }
  }
);
if (
  result.status !== 0 ||
  result.stderr.includes('VERIFICATION FAILED') ||
  !result.stdout.includes(
    'PASS actual pinned Code-OSS NativePolicyService and NativeManagedSettingsService genuine macOS'
  )
) {
  throw Error(JSON.stringify(result));
}
process.stdout.write(result.stdout);
console.log(
  `PASS ${verified.size} compiled dependencies and explicit exact-version package adapter verified; genuine native read-only policy consumer initialization/disposal, no injected-empty snapshot or MDM-change/workbench claim`
);
