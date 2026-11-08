/** Run the pinned Code-OSS NodeJSFileWatcherLibrary over genuine filesystem events. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const referenceOnly = process.argv.includes('--reference-only');
const [treeArg, miteArg] = process.argv.slice(2).filter((value) => value !== '--reference-only');
if (!treeArg || (!referenceOnly && !miteArg)) {
  throw Error('Require compiled Code-OSS tree and Mite binary; optional --reference-only');
}
const tree = path.resolve(treeArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/nodejsWatcherLib.ts'));
const output = 'src/vs/platform/files/node/watcher/nodejs/nodejsWatcherLib.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['nodejsWatcherLib.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS nodejsWatcherLib source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled nodejsWatcherLib dependency drift: ' + file);
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

const url = (file) => JSON.stringify(pathToFileURL(path.join(tree, file)).href);

const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-event-watcher-'));
try {
  const script = path.join(directory, 'watcher.mjs');
  const target = path.join(directory, 'watched-directory');
  fs.mkdirSync(target);
  fs.writeFileSync(
    script,
    `
import ${url('src/nls.messages.js')};
import fs from 'node:fs';
import path from 'node:path';
import {NodeJSFileWatcherLibrary} from ${url(output)};
import {FileChangeType} from ${url('src/vs/platform/files/common/files.js')};
const check=(value,message)=>{if(!value)throw Error(message);};
const directory=${JSON.stringify(target)};
const file=path.join(directory,'consumer.txt');
const events=[];
const logs=[];
let failed=0;
const request={path:directory,recursive:false,excludes:[],correlationId:43};
const watcher=new NodeJSFileWatcherLibrary(request,undefined,changes=>events.push(...changes),()=>failed++,message=>logs.push(message),true);
const deadline=setTimeout(()=>{throw Error('Actual Code-OSS native event consumer timed out');},10000);
async function wait(type,start){
 const until=Date.now()+3500;
 while(!events.slice(start).some(event=>event.type===type&&event.resource.fsPath===file)&&Date.now()<until)await new Promise(resolve=>setTimeout(resolve,10));
 const found=events.slice(start).find(event=>event.type===type&&event.resource.fsPath===file);
 check(found&&found.cId===43,'Actual Code-OSS coalesced/correlated native event '+type);
}
try{
 await watcher.ready;
 check(!watcher.failed&&!failed&&!watcher.isReusingRecursiveWatcher,'Actual fs.watch backend admitted');
 const real=await fs.promises.realpath(directory);
 check(typeof real==='string'&&fs.statSync(real).isDirectory(),'Genuine canonical filesystem path');
 let start=events.length;
 fs.writeFileSync(file,'created');
 await wait(FileChangeType.ADDED,start);
 start=events.length;
 fs.appendFileSync(file,'-changed');
 await wait(FileChangeType.UPDATED,start);
 start=events.length;
 fs.unlinkSync(file);
 await wait(FileChangeType.DELETED,start);
 check(!failed&&!logs.some(message=>message.type==='error'),'Real consumer stayed healthy');
 watcher.dispose();
 const after=events.length;
 fs.writeFileSync(file,'after-disposal');
 await new Promise(resolve=>setTimeout(resolve,150));
 check(events.length===after,'Actual consumer disposal revokes later native events');
 console.log('PASS actual pinned Code-OSS nodejsWatcherLib: genuine canonical path, native create/update/delete, correlation/coalescing and disposal');
}finally{clearTimeout(deadline);watcher.dispose();}
`
  );
  let reference;
  for (const [name, binary] of [
    ['Node', process.execPath],
    ...(referenceOnly ? [] : [['Mite', path.resolve(miteArg)]])
  ]) {
    for (const file of fs.readdirSync(target)) {
      fs.unlinkSync(path.join(target, file));
    }
    const result = spawnSync(binary, [script], {
      encoding: 'utf8',
      timeout: 15000,
      env: { ...process.env, NODE_OPTIONS: '' }
    });
    if (
      result.status !== 0 ||
      result.stderr ||
      !result.stdout.startsWith('PASS actual pinned Code-OSS nodejsWatcherLib')
    ) {
      throw Error(JSON.stringify({ name, ...result }));
    }
    if (reference !== undefined && reference !== result.stdout) {
      throw Error('Code-OSS Node/Mite event watcher output differs');
    }
    reference = result.stdout;
    process.stdout.write(name + ': ' + result.stdout);
  }
  console.log(
    'PASS ' +
      verified.size +
      ' compiled dependencies verified; native event consumer component only, no complete watcher service/workbench claim'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
