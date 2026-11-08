/** Run the pinned Code-OSS suspended-path polling/resumption consumer over real OS files. */
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

const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/baseWatcher.ts'));
const output = 'src/vs/platform/files/node/watcher/baseWatcher.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['baseWatcher.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS baseWatcher source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled baseWatcher dependency drift: ' + file);
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

const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-stat-watcher-'));
try {
  const script = path.join(directory, 'watcher.mjs');
  const target = path.join(directory, 'suspended-missing.txt');
  fs.writeFileSync(
    script,
    `
import ${url('src/nls.messages.js')};
import fs from 'node:fs';
import {BaseWatcher} from ${url(output)};
import {FileChangeType} from ${url('src/vs/platform/files/common/files.js')};
const check=(value,message)=>{if(!value)throw Error(message);};
const file=${JSON.stringify(target)};
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const deadline=setTimeout(()=>{throw Error('Actual Code-OSS suspended-path polling timed out');},15000);
let resumed=0;
// BaseWatcher is abstract. This minimal backend reports an actual missing target;
// all suspension, stat polling, disposal and resumption logic remains unchanged.
class ComponentWatcher extends BaseWatcher {
 trace(){}
 warn(message){throw Error(message);}
 async doWatch(requests){
  for(const request of requests){
   if(!fs.existsSync(request.path))this._onDidWatchFail.fire(request);
   else resumed++;
  }
 }
}
const watcher=new ComponentWatcher();
const request={path:file,recursive:false,excludes:[],correlationId:7};
let baseline;
const missing=new Promise(resolve=>baseline=(current,previous)=>{
 if(current.ino===0&&current.ctimeMs===0){
  check(previous.ino===0&&previous.ctimeMs===0,'Real missing baseline pair');resolve();
 }
});
// A parallel ordinary observer only establishes that the initial missing stat
// completed. It shares the exact same 5007ms/unref options as the real consumer.
fs.watchFile(file,{persistent:false,interval:5007},baseline);
let changed;
const changes=new Promise(resolve=>changed=resolve);
const listener=watcher.onDidChangeFile(changed);
try{
 await watcher.watch([request]);
 while(watcher.isSuspended(request)!=='polling')await sleep(5);
 await missing;
 fs.unwatchFile(file,baseline);
 fs.writeFileSync(file,'actual-created-file');
 const events=await changes;
 check(events.length===1&&events[0].type===FileChangeType.ADDED&&events[0].cId===7&&events[0].resource.fsPath===file,'Actual baseWatcher normalized resumption event');
 while(!resumed)await sleep(5);
 check(watcher.isSuspended(request)===false,'Actual suspended request retired after reappearance');
 await watcher.stop();
 check(watcher.isSuspended(request)===false,'Actual stop clears suspended watches');
 console.log('PASS actual pinned Code-OSS baseWatcher: missing path, 5007ms native polling, correlated ADDED event, resumption and disposal');
}finally{
 clearTimeout(deadline);listener.dispose();await watcher.stop();watcher.dispose();fs.unwatchFile(file);
}
`
  );
  let reference;
  for (const [name, binary] of [
    ['Node', process.execPath],
    ...(referenceOnly ? [] : [['Mite', path.resolve(miteArg)]])
  ]) {
    // Each process must begin with the genuine target missing, never reuse the
    // preceding runtime's successful creation as a substitute baseline.
    if (fs.existsSync(target)) {
      fs.unlinkSync(target);
    }
    const result = spawnSync(binary, [script], {
      encoding: 'utf8',
      timeout: 20000,
      env: { ...process.env, NODE_OPTIONS: '' }
    });
    if (
      result.status !== 0 ||
      result.stderr ||
      !result.stdout.startsWith('PASS actual pinned Code-OSS baseWatcher')
    ) {
      throw Error(JSON.stringify({ name, ...result }));
    }
    if (reference !== undefined && reference !== result.stdout) {
      throw Error('Code-OSS Node/Mite stat watcher output differs');
    }
    reference = result.stdout;
    process.stdout.write(name + ': ' + result.stdout);
  }
  console.log(
    'PASS ' +
      verified.size +
      ' compiled dependencies verified; suspended-path component only, no full watcher service/workbench claim'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
