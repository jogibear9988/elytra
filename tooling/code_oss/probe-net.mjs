/** Run the pinned Code-OSS wake-network consumer against the actual native availability provider. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, nativeProbeArg] = process.argv.slice(2);
if (!treeArg || !nativeProbeArg) {
  throw Error('Require compiled Code-OSS tree and genuine native net probe');
}
const tree = path.resolve(treeArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const source = fs.readFileSync(
  path.join(root, 'vendor/code-oss-tests/agentHostWakeNetworkMonitor.ts')
);
const output = 'src/vs/platform/agentHost/common/agentHostWakeNetworkMonitor.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['agentHostWakeNetworkMonitor.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS AgentHostWakeNetworkMonitor source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled AgentHostWakeNetworkMonitor dependency drift: ' + file);
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

const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-net-'));
try {
  const script = path.join(directory, 'wake-network.mjs');
  fs.writeFileSync(
    script,
    `
import ${url('src/nls.messages.js')};
import native from 'net-test';
import createNet from ${JSON.stringify(pathToFileURL(path.join(root, 'firefly/core/net/index.mjs')).href)};
import {AgentHostWakeNetworkMonitor} from ${url(output)};
const check=(value,message)=>{if(!value)throw Error(message);};
const failure=(code,message)=>Object.assign(Error(message),{code});
const component=createNet(native,()=>true,()=>false,failure,name=>{throw failure('ERR_FIREFLY_UNSUPPORTED_API',name);},native.transport);
let monitor;
try{
 let initial;
 const deadline=Date.now()+2000;
 while(typeof initial!=='boolean'){
  try{initial=component.net.isOnline();}
  catch(error){if(error.code!=='ERR_FIREFLY_NETWORK_STATE_PENDING'||Date.now()>=deadline)throw error;await new Promise(resolve=>setTimeout(resolve,10));}
 }
 let queries=0;
 let last;
 const changes=[];
 monitor=new AgentHostWakeNetworkMonitor(()=>{queries++;last=component.net.isOnline();return last;},value=>changes.push(value));
 monitor.handleWake();
 check(queries===1&&typeof last==='boolean'&&monitor.isOnline===last,'Actual native online snapshot reaches unchanged wake consumer');
 monitor.handleSleep(); // Consumer lifecycle only; no physical sleep/power operation.
 native.collect();
 monitor.handleWake();
 check(queries===2&&monitor.isOnline===last&&changes.every(value=>typeof value==='boolean'),'Actual wake/sleep polling lifecycle');
 monitor.handleSleep();
 monitor.dispose();
 monitor=undefined;
 console.log('PASS actual pinned Code-OSS AgentHostWakeNetworkMonitor: real native net.isOnline, consumer wake/sleep lifecycle and disposal; no physical transitions');
}finally{monitor?.dispose();component.stop();}
`
  );
  const result = spawnSync(path.resolve(nativeProbeArg), [script], {
    encoding: 'utf8',
    timeout: 8000
  });
  if (
    result.status !== 0 ||
    result.stderr ||
    !result.stdout.startsWith('PASS actual pinned Code-OSS AgentHostWakeNetworkMonitor')
  ) {
    throw Error(JSON.stringify(result));
  }
  process.stdout.write(result.stdout);
  console.log(
    'PASS ' +
      verified.size +
      ' compiled dependencies verified; availability consumer only, no Internet reachability/workbench claim'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
