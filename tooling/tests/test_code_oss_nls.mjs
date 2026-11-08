/** Validate actual pinned NLS binding analysis and run generated strings on Node/Mite. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { createNLS, literal } from '../code_oss/nls.mjs';
import { transformSync } from '../runtime-js/node_modules/esbuild/lib/main.js';

const root = fileURLToPath(new URL('../../', import.meta.url));
const mite = path.resolve(process.argv[2]);
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-nls-'));
try {
  const reference = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/nls.ts'));
  const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
  assert.equal(createHash('sha256').update(reference).digest('hex'), pin.files['nls.ts'].sha256);
  const nls = await createNLS();
  const source = `import * as messages from './vs/nls.js';
import { localize as text, localize2 as entry } from './vs/nls.js';
const untouched = "text('fake', 'not a call')";
function shadow(text) { return text('shadow', 'not a translation'); }
export const result = [
 entry('second', 'Two {0}', 2),
 text({key: 'first', comment: ['Translator note']}, 'One {0} ü 😀', 1),
 messages.localize('empty', ''),
 messages.localize2('multiline', \`line one\nline two\`),
 shadow((key,value)=>value), untouched
];`;
  const patched = nls.patch('src/a.ts', source);
  assert.ok(patched.includes("entry(2, 'Two {0}', 2)"));
  assert.ok(patched.includes("text(0, 'One {0} ü 😀', 1)"));
  assert.ok(patched.includes("text('shadow', 'not a translation')"));
  assert.equal(nls.patch('src/b.ts', 'export const ordinary = 1;'), 'export const ordinary = 1;');

  assert.throws(() => nls.patch('src/a.ts', source), /sorted/);

  const artifacts = nls.artifacts();
  const messages = JSON.parse(artifacts.get('src/nls.messages.json'));
  assert.deepEqual(messages, ['One {0} ü 😀', '', 'Two {0}', 'line one\nline two']);
  const metadata = JSON.parse(artifacts.get('src/nls.metadata.json'));
  assert.deepEqual(metadata.keys.a, [
    { key: 'first', comment: ['Translator note'] },
    'empty',
    'second',
    'multiline'
  ]);
  assert.deepEqual(JSON.parse(artifacts.get('src/nls.keys.json')), [
    ['a', ['first', 'empty', 'second', 'multiline']]
  ]);
  assert.ok(!/[^\x00-\x7f]/.test(artifacts.get('src/nls.messages.js').toString()));
  assert.equal(literal("'\\u0041'"), 'A');
  for (const bad of [
    'call()',
    '`value ${danger()}`',
    "'x';danger()",
    '{key: call()}',
    '{get key(){throw 1}}',
    "{key:'a', key:'b'}",
    "{key:'a',comment:[danger()]}"
  ]) {
    assert.throws(() => literal(bad, true), undefined, bad);
  }
  const other = await createNLS();
  assert.equal(other.patch('src/a.ts', source), patched);
  for (const [name, bytes] of artifacts) {
    assert.deepEqual(other.artifacts().get(name), bytes);
  }
  const dynamic = await createNLS();

  assert.throws(
    () =>
      dynamic.patch(
        'src/d.ts',
        "import {localize} from './vs/nls.js'; localize('key', dynamicMessage);"
      ),
    /Nonliteral/
  );

  fs.mkdirSync(path.join(directory, 'vs'));
  fs.writeFileSync(
    path.join(directory, 'vs/nls.js'),
    transformSync(reference.toString(), { loader: 'ts', format: 'esm' }).code
  );
  fs.writeFileSync(path.join(directory, 'package.json'), '{"type":"module"}');
  fs.writeFileSync(
    path.join(directory, 'a.js'),
    transformSync(patched, { loader: 'ts', format: 'esm' }).code
  );
  fs.writeFileSync(path.join(directory, 'nls.messages.js'), artifacts.get('src/nls.messages.js'));
  fs.writeFileSync(
    path.join(directory, 'main.mjs'),
    `import './nls.messages.js';
const {result}=await import('./a.js'); console.log(JSON.stringify(result));`
  );
  let expected;
  for (const binary of [process.execPath, mite]) {
    const result = spawnSync(binary, [path.join(directory, 'main.mjs')], {
      encoding: 'utf8',
      timeout: 20000
    });
    assert.equal(result.status, 0, result.stderr);
    const value = JSON.parse(result.stdout);
    assert.deepEqual(value, [
      { original: 'Two 2', value: 'Two 2' },
      'One 1 ü 😀',
      '',
      { original: 'line one\nline two', value: 'line one\nline two' },
      'not a translation',
      "text('fake', 'not a call')"
    ]);
    if (expected) {
      assert.deepEqual(value, expected);
    }
    expected = value;
  }
  // Exercise the compiler's real fresh-output integration, including fail-before-write.
  const input = path.join(directory, 'source');
  fs.mkdirSync(path.join(input, 'src/vs'), { recursive: true });
  fs.writeFileSync(path.join(input, 'src/a.ts'), source);
  fs.writeFileSync(path.join(input, 'src/vs/nls.ts'), reference);
  fs.writeFileSync(path.join(input, 'package.json'), '{"type":"module"}');

  function compile(output, enabled = true) {
    return spawnSync(
      process.execPath,
      [
        path.join(root, 'tooling/code_oss/compile.mjs'),
        '--source',
        input,
        '--output',
        output,
        ...(enabled ? ['--nls'] : [])
      ],
      { encoding: 'utf8', timeout: 30000 }
    );
  }

  const output = path.join(directory, 'indexed');
  let compilation = compile(output);
  assert.equal(compilation.status, 0, compilation.stderr);
  const record = JSON.parse(fs.readFileSync(path.join(output, 'ELYTRA-COMPILE.json')));
  assert.equal(record.nls.messages, 4);
  assert.equal(record.generated.length, 4);
  for (const generated of record.generated) {
    assert.equal(
      createHash('sha256')
        .update(fs.readFileSync(path.join(output, generated.output)))
        .digest('hex'),
      generated.outputSha256
    );
  }
  assert.deepEqual(
    fs.readFileSync(path.join(output, 'src/a.js')),
    Buffer.from(
      transformSync(patched, {
        loader: 'ts',
        format: 'esm',
        target: 'es2024',
        legalComments: 'inline',
        tsconfigRaw: {
          compilerOptions: { experimentalDecorators: true, useDefineForClassFields: false }
        }
      }).code
    )
  );
  const plain = path.join(directory, 'plain');
  compilation = compile(plain, false);
  assert.equal(compilation.status, 0, compilation.stderr);
  assert.ok(!fs.existsSync(path.join(plain, 'src/nls.messages.json')));
  assert.equal(JSON.parse(fs.readFileSync(path.join(plain, 'ELYTRA-COMPILE.json'))).nls, undefined);
  fs.writeFileSync(
    path.join(input, 'src/a.ts'),
    "import {localize} from './vs/nls.js'; localize('key', dynamicMessage);"
  );
  const invalid = path.join(directory, 'invalid');
  compilation = compile(invalid);
  assert.notEqual(compilation.status, 0);
  assert.match(compilation.stderr, /Nonliteral/);
  assert.ok(!fs.existsSync(invalid));
  fs.writeFileSync(path.join(input, 'src/a.ts'), source);
  fs.writeFileSync(path.join(input, 'src/nls.messages.json'), '[]');
  compilation = compile(invalid);
  assert.notEqual(compilation.status, 0);
  assert.match(compilation.stderr, /collision/);
  assert.ok(!fs.existsSync(invalid));
  console.log(
    'PASS actual NLS analyzer: aliases/shadowing/namespaces, stable indices/catalogs, safe literals, original strings and real Node/Mite lookup'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
