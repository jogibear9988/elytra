/** Verify and execute unchanged Code-OSS protocol consumers against genuine main-process APIs. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, fireflyArg] = process.argv.slice(2);
if (!treeArg || !fireflyArg) {
  throw Error('Require compiled Code-OSS tree and genuine Firefly executable');
}
const tree = path.resolve(treeArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
const outputs = [
  'src/vs/platform/protocol/electron-main/protocolMainService.js',
  'src/vs/platform/protocol/electron-main/managedRemoteResourceProtocol.js'
];
if (report.kind !== 'diagnostic-source-tree' || report.adaptations?.revision !== pin.revision) {
  throw Error('Pinned Code-OSS protocol source/compiled revision drift');
}
for (const output of outputs) {
  const name = path.posix.basename(output, '.js') + '.ts';
  const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests', name));
  if (
    hash(source) !== pin.files[name]?.sha256 ||
    records.get(output)?.sourceSha256 !== hash(source)
  ) {
    throw Error('Pinned Code-OSS protocol source/compiled input drift: ' + name);
  }
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  if (file.startsWith('../') || path.posix.isAbsolute(file)) {
    throw Error('Compiled protocol dependency escaped tree: ' + file);
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled protocol dependency drift: ' + file);
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
verify('src/vs/base/common/uri.js');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-protocol-'));
try {
  for (const name of ['app', 'extensions', 'global', 'workspace']) {
    fs.mkdirSync(path.join(directory, name));
  }
  const secret = path.join(directory, 'owned-secret.txt');
  fs.writeFileSync(secret, 'owned file must never escape the Code-OSS file denial');
  const result = spawnSync(
    path.resolve(fireflyArg),
    ['--electron-compat', path.join(root, 'firefly/tests/protocol/consumer-entry.mjs')],
    {
      encoding: 'utf8',
      timeout: 20000,
      env: {
        ...process.env,
        QT_QPA_PLATFORM: 'offscreen',
        ELYTRA_PROTOCOL_CONSUMER: pathToFileURL(path.join(tree, outputs[0])).href,
        ELYTRA_PROTOCOL_URI: pathToFileURL(path.join(tree, 'src/vs/base/common/uri.js')).href,
        ELYTRA_PROTOCOL_NLS: pathToFileURL(path.join(tree, 'src/nls.messages.js')).href,
        ELYTRA_PROTOCOL_FIXTURE: directory,
        ELYTRA_PROTOCOL_SECRET: pathToFileURL(secret).href
      }
    }
  );
  if (
    result.status !== 0 ||
    result.stderr.includes('VERIFICATION FAILED') ||
    !result.stdout.includes(
      'PASS actual pinned Code-OSS ProtocolMainService genuine main-process protocol consumers'
    )
  ) {
    throw Error(JSON.stringify(result));
  }
  process.stdout.write(result.stdout);
  console.log(
    `PASS ${verified.size} compiled dependencies verified; native-backed protocol registration, genuine main net.fetch file denial and remote Response bytes; no renderer/workbench/remote-service acceptance claim`
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
