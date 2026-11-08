/** Run the unchanged Code-OSS nodeRequest consumer over actual native net HTTP transport. */
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

const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/nodeRequestService.ts'));
const output = 'src/vs/platform/request/node/requestService.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['nodeRequestService.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS nodeRequest source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled nodeRequest dependency drift: ' + file);
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

if (!process.env.ELYTRA_NET_TEST) {
  throw Error('Require ELYTRA_NET_TEST owned-loopback endpoint configuration');
}
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-net-request-'));
try {
  const script = path.join(directory, 'request.mjs');
  fs.writeFileSync(
    script,
    `
import ${url('src/nls.messages.js')};
import native from 'net-test';
import createNet from ${JSON.stringify(pathToFileURL(path.join(root, 'firefly/core/net/index.mjs')).href)};
import {nodeRequest} from ${url(output)};
import {streamToBuffer} from ${url('src/vs/base/common/buffer.js')};
import {CancellationToken,CancellationTokenSource} from ${url('src/vs/base/common/cancellation.js')};
const check=(value,message)=>{if(!value)throw Error(message);};
const failure=(code,message)=>Object.assign(Error(message),{code});
const component=createNet(native,()=>true,()=>false,failure,name=>{throw failure('ERR_FIREFLY_UNSUPPORTED_API',name);},native.transport);
const config=JSON.parse(process.env.ELYTRA_NET_TEST);
const base={getRawRequest:()=>component.net.request,isChromiumNetwork:true,strictSSL:true,agent:null};
const deadline=setTimeout(()=>{throw Error('Actual Code-OSS net request timed out');},10000);
try{
 let result=await nodeRequest({...base,url:config.origin+'/echo',type:'POST',data:'actual-CodeOSS-POST',headers:{'X-Probe':'codeoss'}},CancellationToken.None);
 let echo=JSON.parse((await streamToBuffer(result.stream)).toString());
 check(result.res.statusCode===200&&echo.bodyHex===Buffer.from('actual-CodeOSS-POST').toString('hex')&&echo.probe==='codeoss','Unchanged Code-OSS request bytes/response adapter');
 result=await nodeRequest({...base,url:config.origin+'/redirect?to='+encodeURIComponent(config.crossPort+'/echo'),headers:{Authorization:'Bearer codeoss-origin'},followRedirects:2},CancellationToken.None);
 echo=JSON.parse((await streamToBuffer(result.stream)).toString());
 check(echo.authorization===null,'Code-OSS manual redirect strips cross-origin credentials');
 result=await nodeRequest({...base,url:config.origin+'/gzip'},CancellationToken.None);
 check((await streamToBuffer(result.stream)).toString()==='decoded Grüße 日本語','Code-OSS Chromium branch receives already decompressed native response');
 const cts=new CancellationTokenSource();
 const key=(config.runId||'run')+'-codeoss-cancel';
 const canceled=nodeRequest({...base,url:config.origin+'/hold/'+key},cts.token).then(()=>undefined,error=>error);
 let seen=false;
 for(let attempt=0;attempt<100;attempt++){
  if((await(await component.net.fetch(config.origin+'/seen/'+key)).json()).seen){seen=true;break;}
  await new Promise(resolve=>setTimeout(resolve,5));
 }
 check(seen,'Independent server observed actual cancellable Code-OSS request');
 cts.cancel();
 check((await canceled)?.name==='Canceled','Actual Code-OSS cancellation aborts ClientRequest');
 await component.net.fetch(config.origin+'/release/'+key);
 cts.dispose();
 console.log('PASS actual pinned Code-OSS nodeRequest: POST bytes, manual secure redirects, decompression, stream adapter and cancellation');
}finally{clearTimeout(deadline);component.stop();}
`
  );
  const result = spawnSync(path.resolve(nativeProbeArg), [script], {
    encoding: 'utf8',
    timeout: 15000
  });
  if (
    result.status !== 0 ||
    result.stderr ||
    !result.stdout.startsWith('PASS actual pinned Code-OSS nodeRequest')
  ) {
    throw Error(JSON.stringify(result));
  }
  process.stdout.write(result.stdout);
  console.log(
    'PASS ' +
      verified.size +
      ' compiled dependencies verified; request consumer only, no full proxy/session/workbench claim'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
