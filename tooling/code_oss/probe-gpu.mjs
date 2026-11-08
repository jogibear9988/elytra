/** Run the pinned Code-OSS GPU status consumer inside genuine Firefly with its actual native-backed app constructor collaborator. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, fireflyArg] = process.argv.slice(2);
if (!treeArg || !fireflyArg) {
  throw Error('Require compiled Code-OSS tree and exact-object offscreen Firefly executable');
}
const tree = path.resolve(treeArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/gpuProcessMainService.ts'));
const output = 'src/vs/platform/gpu/electron-main/gpuProcessMainService.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['gpuProcessMainService.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS GPUProcessMainService source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled GPUProcessMainService dependency drift: ' + file);
  }
  verified.add(file);
  for (const match of bytes.toString().matchAll(/(?:\bfrom\s*|\bimport\s*)["']([^"']+)["']/g)) {
    if (match[1].startsWith('.')) {
      verify(path.posix.normalize(path.posix.join(path.posix.dirname(file), match[1])));
    }
  }
}

verify(output);
verify('src/nls.messages.js');

const result = spawnSync(
  path.resolve(fireflyArg),
  ['--electron-compat', path.join(root, 'firefly/tests/gpu/consumer-entry.mjs')],
  {
    encoding: 'utf8',
    timeout: 15000,
    env: {
      ...process.env,
      QT_QPA_PLATFORM: 'offscreen',
      ELYTRA_GPU_CONSUMER: pathToFileURL(path.join(tree, output)).href
    }
  }
);
if (
  result.status !== 0 ||
  result.stderr.includes('VERIFICATION FAILED') ||
  !result.stdout.includes(
    'PASS actual pinned Code-OSS GPUProcessMainService native-backed GPU status consumer'
  )
) {
  throw Error(JSON.stringify(result));
}
process.stdout.write(result.stdout);
console.log(
  'PASS ' +
    verified.size +
    ' compiled dependencies verified; actual Firefly/Electron import graph with genuine app status; synthetic consumer lifecycle events, no device/workbench acceptance claim'
);
