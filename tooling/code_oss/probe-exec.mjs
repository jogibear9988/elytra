/** Run the actual compiled Code-OSS terminal-encoding consumer through a genuine OS shell. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { execSync, spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const referenceOnly = process.argv.includes('--reference-only');
const [treeArg, miteArg] = process.argv.slice(2).filter((value) => value !== '--reference-only');
if (!treeArg || (!miteArg && !referenceOnly)) {
  throw Error('Require compiled Code-OSS tree and Mite binary; optional --reference-only');
}
const tree = path.resolve(treeArg);

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));
if (report.kind !== 'diagnostic-source-tree' || report.adaptations?.revision !== pin.revision) {
  throw Error('Wrong Code-OSS compiled baseline');
}
const original = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/terminalEncoding.ts'));
const output = 'src/vs/base/node/terminalEncoding.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  hash(original) !== pin.files['terminalEncoding.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(original)
) {
  throw Error('Pinned terminal-encoding source drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error(`Compiled terminal-encoding dependency drift: ${file}`);
  }
  verified.add(file);
  for (const match of bytes.toString().matchAll(/(?:\bfrom\s*|\bimport\s*)["']([^"']+)["']/g)) {
    if (match[1].startsWith('.')) {
      verify(path.posix.normalize(path.posix.join(path.posix.dirname(file), match[1])));
    }
  }
}

verify(output);
const environment = { ...process.env };
// The real consumer must execute its OS command, not accept the environment shortcut.
for (const key of Object.keys(environment)) {
  if (key.toUpperCase() === 'VSCODE_CLI_ENCODING') {
    delete environment[key];
  }
}
const command = process.platform === 'win32' ? 'chcp' : 'locale charmap';
const native = execSync(command, { env: environment, encoding: 'utf8', timeout: 10000 });
if (!native.trim()) {
  throw Error('Real OS encoding command did not produce output');
}
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-exec-'));
try {
  const script = path.join(directory, 'probe.mjs');
  fs.writeFileSync(
    script,
    `
import {resolveTerminalEncoding} from ${JSON.stringify(pathToFileURL(path.join(tree, output)).href)};
if(process.env.VSCODE_CLI_ENCODING!==undefined)throw Error('Environment shortcut remains set');
const encoding=await resolveTerminalEncoding(true);
if(typeof encoding!=='string'||!encoding)throw Error('Missing actual terminal encoding');
console.log('PASS actual compiled Code-OSS terminal encoding '+JSON.stringify({platform:process.platform,encoding}));
`
  );
  let reference;
  for (const [name, binary] of [
    ['Node', process.execPath],
    ...(referenceOnly ? [] : [['Mite', path.resolve(miteArg)]])
  ]) {
    const result = spawnSync(binary, [script], {
      env: environment,
      encoding: 'utf8',
      timeout: 20000
    });
    const rawMessage = `Detected raw terminal encoding: ${native}`;
    if (
      result.status !== 0 ||
      result.stderr ||
      !result.stdout.includes(`Running "${command}" to detect terminal encoding...`) ||
      !result.stdout.includes(rawMessage) ||
      !result.stdout.includes('PASS actual compiled Code-OSS terminal encoding')
    ) {
      throw Error(
        JSON.stringify({ name, expectedCommand: command, expectedRaw: native, ...result })
      );
    }
    if (reference !== undefined && result.stdout !== reference) {
      throw Error(
        `Actual Node/Mite consumer output differs: ${JSON.stringify({ reference, actual: result.stdout })}`
      );
    }
    reference = result.stdout;
    process.stdout.write(`${name}: ${result.stdout}`);
  }
  console.log(
    `PASS ${verified.size} actual compiled dependencies verified; OS shell stdout reaches unchanged Code-OSS consumer`
  );
  console.log('Component acceptance only; no terminal/PTY, process-tree or workbench acceptance');
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
