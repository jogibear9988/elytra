/** Verify unchanged pinned Code-OSS IPC and exchange binary channels between real OS processes. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash, randomUUID } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, miteArg] = process.argv.slice(2);
if (!treeArg || !miteArg) {
  throw Error('Require compiled Code-OSS tree and actual Mite executable');
}
const tree = fs.realpathSync(treeArg);
const mite = fs.realpathSync(miteArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const output = 'src/vs/base/parts/ipc/node/ipc.net.js';
const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/ipc-net-node.ts'));
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['ipc-net-node.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS local IPC source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  if (file.startsWith('../') || path.posix.isAbsolute(file)) {
    throw Error('Dependency escaped compiled tree');
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled IPC dependency drift: ' + file);
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

verify(output);
verify('src/nls.messages.js');

// Both directions use independent processes and the original protocol. A TCP
// replacement cannot satisfy a filesystem endpoint or a Windows pipe name.
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-ipc-'));
const entry = path.join(root, 'mite/tests/net/code-oss-ipc.mjs');
const children = new Set();

function launch(executable, role, endpoint) {
  const child = spawn(executable, [entry, tree, role, endpoint], {
    stdio: ['ignore', 'pipe', 'pipe']
  });
  children.add(child);
  const state = { stdout: '', stderr: '', child };

  child.stdout.on('data', (data) => {
    state.stdout += data;
  });

  child.stderr.on('data', (data) => {
    state.stderr += data;
  });

  state.done = new Promise((resolve, reject) => {
    child.once('error', reject);

    child.once('close', (code, signal) => {
      children.delete(child);
      if (
        code !== 0 ||
        signal ||
        state.stderr ||
        !state.stdout.includes(`PASS Code-OSS local IPC ${role}`)
      ) {
        reject(Error(JSON.stringify({ code, signal, stdout: state.stdout, stderr: state.stderr })));
      } else {
        resolve();
      }
    });
  });
  // The server may fail before readiness; retain its diagnostic without an
  // unhandled rejection while the readiness promise is still waiting.
  state.done.catch(() => {});

  state.ready = new Promise((resolve, reject) => {
    const timeout = setTimeout(
      () => reject(Error('IPC server readiness timeout: ' + state.stderr)),
      10000
    );

    const inspect = () => {
      if (state.stdout.includes('READY\n')) {
        clearTimeout(timeout);
        resolve();
      }
    };

    child.stdout.on('data', inspect);

    child.once('close', () => {
      clearTimeout(timeout);
      reject(Error('IPC exited before READY: ' + state.stderr));
    });

    inspect();
  });

  state.ready.catch(() => {});

  return state;
}

try {
  for (const [serverExecutable, clientExecutable] of [
    [mite, mite],
    [mite, process.execPath],
    [process.execPath, mite]
  ]) {
    const endpoint =
      process.platform === 'win32'
        ? `\\\\.\\pipe\\elytra-${randomUUID()}`
        : path.join(directory, randomUUID().slice(0, 8) + '.sock');
    const server = launch(serverExecutable, 'server', endpoint);
    await server.ready;
    const client = launch(clientExecutable, 'client', endpoint);
    await Promise.all([server.done, client.done]);
    if (process.platform !== 'win32' && fs.existsSync(endpoint)) {
      throw Error('Owned Unix socket not cleaned up');
    }
    console.log(
      `PASS unchanged Code-OSS local IPC ${path.basename(serverExecutable)} -> ${path.basename(clientExecutable)} binary channels and disposal`
    );
  }
  console.log(
    `PASS ${verified.size} compiled dependencies verified; local IPC component only, no workbench claim`
  );
} finally {
  for (const child of children) {
    child.kill();
  }
  fs.rmSync(directory, { recursive: true, force: true });
}
