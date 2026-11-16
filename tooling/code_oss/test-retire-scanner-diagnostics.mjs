/** Exercise exact CA-29 retirement only in a compact temporary owned fixture. */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import assert from 'node:assert/strict';
import { prepareScannerDiagnosticsRetirement } from './retire-scanner-diagnostics.mjs';
import { verifiedRefresh } from './compile-refresh.mjs';
const [stage, overlay] = process.argv.slice(2).map((value) => fs.realpathSync(value));
// The actual stage/overlay are read-only inputs. Verify them before making a fixture.
prepareScannerDiagnosticsRetirement(stage, overlay);
const base = fs.mkdtempSync(path.join(fs.realpathSync(os.tmpdir()), 'elytra-scanner-retirement-'));
try {
  const copy = path.join(base, 'stage');
  const over = path.join(base, 'overlay');
  fs.mkdirSync(copy);
  fs.mkdirSync(over);
  const original = JSON.parse(fs.readFileSync(path.join(stage, 'ELYTRA-COMPILE.json')));
  const selected = original.inputs.filter((item) =>
    /common\/(elytraScannerDiagnostics|extensionsScannerService)\.ts$/.test(item.source)
  );
  assert.equal(selected.length, 2);
  const metadata = {
    schema: original.schema,
    kind: original.kind,
    compiler: original.compiler,
    inputs: selected,
    adaptations: original.adaptations,
    packageAdaptations: original.packageAdaptations
  };

  function copyFile(root, destination, relative) {
    const target = path.join(destination, relative);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.copyFileSync(path.join(root, relative), target);
  }

  for (const item of selected) {
    copyFile(stage, copy, item.output);
  }
  for (const item of original.adaptations.files) {
    copyFile(overlay, over, item.path);
  }
  fs.copyFileSync(
    path.join(overlay, 'ELYTRA-ADAPTATIONS.json'),
    path.join(over, 'ELYTRA-ADAPTATIONS.json')
  );
  for (const adapter of original.packageAdaptations) {
    for (const item of [
      adapter,
      ...(adapter.additionalFiles ?? []),
      ...(adapter.auditFiles ?? [])
    ]) {
      copyFile(stage, copy, item.path);
      let directory = path.dirname(fs.realpathSync(path.join(stage, item.path)));
      while (!fs.existsSync(path.join(directory, 'package.json'))) {
        directory = path.dirname(directory);
      }
      copyFile(stage, copy, path.relative(stage, path.join(directory, 'package.json')));
    }
  }
  const manifest = path.join(copy, 'ELYTRA-COMPILE.json');
  fs.writeFileSync(manifest, JSON.stringify(metadata, null, 2) + '\n');
  const baseline = fs.readFileSync(manifest);
  const beforePackages = JSON.stringify(metadata.packageAdaptations);
  const helper = path.join(
    over,
    'src/vs/platform/extensionManagement/common/elytraScannerDiagnostics.ts'
  );
  const helperBytes = fs.readFileSync(helper);
  fs.appendFileSync(helper, '\n');

  assert.throws(() => prepareScannerDiagnosticsRetirement(copy, over));

  fs.writeFileSync(helper, helperBytes);
  const unknown = path.join(over, 'unknown');
  fs.writeFileSync(unknown, 'unowned');

  assert.throws(() => prepareScannerDiagnosticsRetirement(copy, over));

  fs.unlinkSync(unknown);
  const plan = prepareScannerDiagnosticsRetirement(copy, over);
  fs.appendFileSync(manifest, ' ');

  assert.throws(() => plan.apply());

  assert(fs.existsSync(helper));
  fs.writeFileSync(manifest, baseline);
  const result = prepareScannerDiagnosticsRetirement(copy, over).apply();
  const after = verifiedRefresh(copy).previous;
  assert.equal(after.inputs.length, 1);
  assert.equal(JSON.stringify(after.packageAdaptations), beforePackages);
  assert(!fs.existsSync(helper));
  assert(
    !fs.existsSync(
      path.join(over, 'src/vs/platform/extensionManagement/common/extensionsScannerService.ts')
    )
  );
  assert(
    !fs.existsSync(
      path.join(copy, 'src/vs/platform/extensionManagement/common/elytraScannerDiagnostics.js')
    )
  );
  const nextOverlay = JSON.parse(fs.readFileSync(path.join(over, 'ELYTRA-ADAPTATIONS.json')));
  assert.equal(nextOverlay.files.length, original.adaptations.files.length - 2);
  assert.equal(nextOverlay.adaptations.length, original.adaptations.adaptations.length - 1);
  assert(
    fs.readFileSync(path.join(copy, result.backup, 'ELYTRA-COMPILE.before.json')).equals(baseline)
  );

  assert.throws(() => prepareScannerDiagnosticsRetirement(copy, over));

  console.log(
    'PASS scanner retirement: exact provenance, drift/unknown/reentry refusal, single compiled helper removal, unchanged six adapters, bounded backups'
  );
} finally {
  fs.rmSync(base, { recursive: true, force: true });
}
