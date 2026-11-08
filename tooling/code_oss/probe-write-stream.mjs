/** Exercise the unchanged compiled Code-OSS ZIP writer on real native file streams. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

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
const original = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/zip.ts'));
const output = 'src/vs/base/node/zip.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  hash(original) !== pin.files['zip.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(original)
) {
  throw Error('Pinned ZIP source drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error(`Compiled ZIP dependency drift: ${file}`);
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
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-write-stream-'));
try {
  const source = path.join(directory, 'source.bin');
  const payload = Buffer.alloc(196613);
  for (let index = 0; index < payload.length; index++) {
    payload[index] = index % 251;
  }
  fs.writeFileSync(source, payload);
  for (const [name, binary] of [
    ['Node', process.execPath],
    ...(referenceOnly ? [] : [['Mite', path.resolve(miteArg)]])
  ]) {
    const archive = path.join(directory, `${name}.zip`);
    const script = path.join(directory, 'driver.mjs');
    fs.writeFileSync(
      script,
      `
import ${JSON.stringify(pathToFileURL(path.join(tree, 'src/nls.messages.js')).href)};
import {zip} from ${JSON.stringify(pathToFileURL(path.join(tree, output)).href)};
import {CancellationToken} from ${JSON.stringify(pathToFileURL(path.join(tree, 'src/vs/base/common/cancellation.js')).href)};
import fs from 'node:fs';
const check=(value,message)=>{if(!value)throw Error(message);};
const archive=${JSON.stringify(archive)};
const input=${JSON.stringify(source)};
const progress=[];
const result=await zip(archive,[
 {path:'binary.bin',contents:fs.readFileSync(input),compress:false},
 {path:'unicode.txt',contents:'Code-OSS Grüße 🦋\\0',compress:false},
 {path:'empty.bin',contents:Buffer.alloc(0),compress:false},
 {path:'prefix.bin',localPath:input,localPathSize:65539,compress:false}
],{onProgress:(bytes,name)=>progress.push([bytes,name])});
check(result===archive,'Actual writer result path failed');
check(progress.length>=4&&progress.at(-1)[0]>196613,'Actual writer progress missing');
let cancelled=false;
try{await zip(archive+'.cancelled',[],{token:CancellationToken.Cancelled});}catch(error){cancelled=error.name==='Canceled';}
check(cancelled&&!fs.existsSync(archive+'.cancelled'),'Actual pre-cancellation semantics failed');
let failed=false;
try{await zip(archive+'/missing/file.zip',[{path:'x',contents:'x',compress:false}]);}catch{failed=true;}
check(failed,'Actual writer native open error was swallowed');
console.log('PASS actual compiled Code-OSS zip writer finish/close, progress, read-stream prefix, cancellation and open error');
`
    );
    const result = spawnSync(binary, [script], { encoding: 'utf8', timeout: 20000 });
    if (result.status !== 0 || !result.stdout.includes('PASS actual compiled Code-OSS')) {
      throw Error(JSON.stringify({ name, ...result }));
    }
    const verification = spawnSync(
      'python3',
      [
        '-c',
        `
import sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as archive:
 assert archive.testzip() is None
 assert archive.namelist()==['binary.bin','unicode.txt','empty.bin','prefix.bin']
 expected=bytes(index%251 for index in range(196613))
 assert archive.read('binary.bin')==expected
 assert archive.read('unicode.txt')=='Code-OSS Grüße 🦋\\0'.encode()
 assert archive.read('empty.bin')==b''
 assert archive.read('prefix.bin')==expected[:65539]
`,
        archive
      ],
      { encoding: 'utf8', timeout: 10000 }
    );
    if (verification.status !== 0) {
      throw Error(JSON.stringify(verification));
    }
    process.stdout.write(`${name}: ${result.stdout}`);
    console.log(`${name}: PASS independent Python ZIP/CRC and exact extracted byte verification`);
  }
  console.log(
    `PASS ${verified.size} actual compiled ZIP dependencies verified; extension installation/full ZIP API is not accepted`
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
