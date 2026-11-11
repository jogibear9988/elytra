/** Exercise real compiler refresh ownership without a Code-OSS clone or GUI. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const root = fs.realpathSync(fs.mkdtempSync(path.join(os.tmpdir(), 'code-oss-refresh-')));
const compiler = fileURLToPath(new URL('../code_oss/compile.mjs', import.meta.url));
const source = path.join(root, 'source');
const output = path.join(root, 'output');

function run(...extra) {
  return spawnSync(process.execPath, [compiler, '--source', source, '--output', output, ...extra], {
    encoding: 'utf8'
  });
}

try {
  fs.mkdirSync(path.join(source, 'src'), { recursive: true });
  fs.writeFileSync(path.join(source, 'package.json'), '{"type":"module"}\n');
  fs.writeFileSync(path.join(source, 'src/main.ts'), 'export const value: number = 1;\n');
  assert.equal(run().status, 0);
  fs.writeFileSync(path.join(output, '.DS_Store'), 'preserved Finder metadata');
  const packageBefore = fs.statSync(path.join(output, 'package.json')).mtimeMs;
  fs.writeFileSync(path.join(source, 'src/main.ts'), 'export const value: number = 2;\n');
  const refreshed = run('--refresh');
  assert.equal(refreshed.status, 0, refreshed.stderr);
  assert.match(fs.readFileSync(path.join(output, 'src/main.js'), 'utf8'), /value = 2/);
  assert.equal(
    fs.readFileSync(path.join(output, '.DS_Store'), 'utf8'),
    'preserved Finder metadata'
  );
  assert.equal(fs.statSync(path.join(output, 'package.json')).mtimeMs, packageBefore);
  console.log('PASS real compiler refresh changes only owned output and preserves Finder metadata');
  const manifest = fs.readFileSync(path.join(output, 'ELYTRA-COMPILE.json'));
  const accepted = fs.readFileSync(path.join(output, 'src/main.js'));
  fs.writeFileSync(path.join(output, 'src/main.js'), '// user edit\n');
  const rejected = run('--refresh');
  assert.notEqual(rejected.status, 0);
  assert.match(rejected.stderr, /Refuse edited/);
  assert.deepEqual(fs.readFileSync(path.join(output, 'ELYTRA-COMPILE.json')), manifest);
  console.log('PASS edited compiled output rejects refresh before manifest or file mutation');
  fs.writeFileSync(path.join(output, 'src/main.js'), accepted);
  fs.writeFileSync(path.join(source, 'src/new.ts'), 'export const added = true;\n');
  fs.writeFileSync(path.join(output, 'src/new.js'), '// unrelated user file\n');
  const collision = run('--refresh');
  assert.notEqual(collision.status, 0);
  assert.match(collision.stderr, /Unowned output collision/);
  assert.equal(
    fs.readFileSync(path.join(output, 'src/new.js'), 'utf8'),
    '// unrelated user file\n'
  );
  console.log('PASS refresh refuses collisions with unowned files');
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}
