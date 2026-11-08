/** Run the unchanged compiled Code-OSS encryption service with genuine native safeStorage. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, executableArg, ...hostArguments] = process.argv.slice(2);
if (!treeArg || !executableArg) {
  throw Error('Require compiled Code-OSS tree, native component host and optional host arguments');
}
const tree = path.resolve(treeArg);
const executable = path.resolve(executableArg);

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));
if (report.kind !== 'diagnostic-source-tree' || report.adaptations?.revision !== pin.revision) {
  throw Error('Wrong Code-OSS compiled baseline');
}
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
for (const name of ['encryptionMainService.ts', 'encryptionService.ts']) {
  const original = fs.readFileSync(path.join(root, 'vendor/code-oss-tests', name));
  const record = pin.files[name];
  const compiled = records.get(record?.upstream.replace(/\.ts$/, '.js'));
  if (hash(original) !== record?.sha256 || compiled?.sourceSha256 !== record.sha256) {
    throw Error(`Pinned encryption source drift: ${name}`);
  }
}

const checked = new Set();

/** Hash the actual component and its static relative compiled dependencies, not replacement seams. */
function verify(output) {
  if (checked.has(output)) {
    return;
  }
  const record = records.get(output);
  const bytes = fs.readFileSync(path.join(tree, output));
  if (!record || hash(bytes) !== record.outputSha256) {
    throw Error(`Compiled encryption dependency drift: ${output}`);
  }
  checked.add(output);
  // esbuild emits static ESM imports with string literals; no source rewriting is performed.
  for (const match of bytes.toString().matchAll(/(?:\bfrom\s*|\bimport\s*)["']([^"']+)["']/g)) {
    if (match[1].startsWith('.')) {
      verify(path.posix.normalize(path.posix.join(path.posix.dirname(output), match[1])));
    }
  }
}

const component = 'src/vs/platform/encryption/electron-main/encryptionMainService.js';
verify(component);
verify('src/nls.messages.js');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-safe-storage-'));
try {
  const fixture = path.join(directory, 'encrypted.json');
  const script = path.join(directory, 'probe.mjs');
  const values = ['', 'Code-OSS Grüße 🦋', 'embedded\0NUL', 'line one\nline two'];
  fs.writeFileSync(
    script,
    `
import ${JSON.stringify(pathToFileURL(path.join(tree, 'src/nls.messages.js')).href)};
import { EncryptionMainService } from ${JSON.stringify(pathToFileURL(path.join(tree, component)).href)};
import { app, safeStorage } from 'electron';
import fs from 'node:fs';
const check=(condition,message)=>{if(!condition)throw Error(message);};
const errors=[];
const service=new EncryptionMainService({trace(){},error(error){errors.push(error);}});
const values=${JSON.stringify(values)};
const fixture=${JSON.stringify(fixture)};
const phase=${JSON.stringify('__PHASE__')};
app.whenReady().then(async()=>{
 try {
  check(await service.isEncryptionAvailable()===true,'Native encryption is unavailable; no successful crypto acceptance');
  const provider=await service.getKeyStorageProvider();
  check(provider===(process.platform==='darwin'?'keychain_access':process.platform==='win32'?'dpapi':safeStorage.getSelectedStorageBackend()),'Actual provider classification failed');
  if(phase==='write'){
   const encrypted=[];
   for(const value of values){
    const wire=await service.encrypt(value);
    const parsed=JSON.parse(wire);
    check(parsed.type==='Buffer'&&Array.isArray(parsed.data),'Actual Buffer serialization failed');
    check(await service.decrypt(wire)===value,'Actual service roundtrip failed');
    encrypted.push(wire);
   }
   fs.writeFileSync(fixture,JSON.stringify(encrypted));
  }else{
   const encrypted=JSON.parse(fs.readFileSync(fixture,'utf8'));
   check(encrypted.length===values.length,'Missing persisted ciphertext');
   for(let index=0;index<values.length;index++)check(await service.decrypt(encrypted[index])===values[index],'Actual cross-process service decrypt failed');
  }
  for(const invalid of ['not json','{}']){
   let rejected=false;try{await service.decrypt(invalid);}catch{rejected=true;}
   check(rejected,'Malformed serialized secret accepted');
  }
  check(errors.length>=2,'Actual error logging did not run');
  if(process.platform==='darwin'||process.platform==='win32'){
   let rejected=false;try{await service.setUsePlainTextEncryption();}catch{rejected=true;}
   check(rejected,'Actual platform plain-text refusal missing');
   check(safeStorage.getSelectedStorageBackend===undefined,'Linux-only API fabricated on another platform');
  }
  console.log('PASS actual compiled Code-OSS EncryptionMainService '+phase+' with real native storage');
 }catch(error){console.error(error);process.exitCode=1;}
});
`
  );
  const template = fs.readFileSync(script, 'utf8');
  for (const phase of ['write', 'read']) {
    fs.writeFileSync(script, template.replace('"__PHASE__"', JSON.stringify(phase)));
    const args = hostArguments.map((value) => value.replaceAll('{directory}', directory));
    const result = spawnSync(executable, [...args, script], { encoding: 'utf8', timeout: 20000 });
    const expected = `PASS actual compiled Code-OSS EncryptionMainService ${phase}`;
    if (result.status !== 0 || !result.stdout.includes(expected)) {
      throw Error(JSON.stringify({ phase, ...result }));
    }
    process.stdout.write(result.stdout);
  }
  console.log(
    `PASS ${checked.size} compiled dependencies verified; component host lifecycle is not full Firefly app acceptance`
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
