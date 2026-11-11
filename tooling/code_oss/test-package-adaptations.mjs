/** Small admission/refusal checks for additive package refresh; no compiled editor mutation. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { planPolicyWatcher, applyPolicyWatcher } from './policy-watcher.mjs';
import { planNativeKeymap, applyNativeKeymap } from './native-keymap.mjs';
import { planSQLite, applySQLite } from './sqlite.mjs';
import { verifyPackageAdditions } from './package-adaptations.mjs';
const repository = fileURLToPath(new URL('../../', import.meta.url));
const root = fs.realpathSync(fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-package-additions-')));
try {
  const packages = [
    ['policy-watcher', '@vscode/policy-watcher', ['package.json', 'index.js']],
    ['native-keymap', 'native-keymap', ['package.json', 'index.js']],
    ['vscode-sqlite3', '@vscode/sqlite3', ['package.json', 'lib/sqlite3.js']]
  ];
  for (const [source, name, files] of packages) {
    for (const file of files) {
      const target = path.join(root, 'node_modules', name, file);
      fs.mkdirSync(path.dirname(target), { recursive: true });
      fs.copyFileSync(path.join(repository, 'vendor', source, file), target);
    }
  }
  const plans = [
    { plan: planPolicyWatcher(root), apply: applyPolicyWatcher },
    { plan: planNativeKeymap(root), apply: applyNativeKeymap },
    { plan: planSQLite(root), apply: applySQLite }
  ];
  for (const { plan, apply } of verifyPackageAdditions(root, [], plans.slice(0, 1))) {
    apply(root, plan);
  }
  const previous = [plans[0].plan.record];
  const additions = verifyPackageAdditions(root, previous, plans);
  assert.deepEqual(
    additions.map((item) => item.plan.record.name),
    ['native-keymap', '@vscode/sqlite3']
  );

  assert.throws(() => verifyPackageAdditions(root, previous, plans.slice(1)));

  for (const name of ['index.js', 'package.json']) {
    const file = path.join(root, 'node_modules/native-keymap', name);
    const bytes = fs.readFileSync(file);
    fs.appendFileSync(file, '\n');

    assert.throws(() => verifyPackageAdditions(root, previous, plans));

    fs.writeFileSync(file, bytes);
  }
  for (const { plan, apply } of additions) {
    apply(root, plan);
  }
  const records = plans.map((item) => item.plan.record);
  assert.equal(verifyPackageAdditions(root, records, plans).length, 0);
  const changed = path.join(root, plans[2].plan.record.path);
  fs.appendFileSync(changed, '\n// user edit');

  assert.throws(() => verifyPackageAdditions(root, records, plans));

  console.log(
    'PASS additive native package migration preserves previous adapters and refuses loader/manifest drift'
  );
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}
