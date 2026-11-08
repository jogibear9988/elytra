/** Run the pinned, compiled Code-OSS ipc.cp Client/Server over real runtime self-fork IPC. */
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

const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/ipc-cp-node.ts'));
const output = 'src/vs/base/parts/ipc/node/ipc.cp.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['ipc-cp-node.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS ipc.cp source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled ipc.cp dependency drift: ' + file);
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

const imports = `import ${url('src/nls.messages.js')};\n`;
const ipc = url(output);
const buffer = url('src/vs/base/common/buffer.js');
const event = url('src/vs/base/common/event.js');
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-fork-'));
try {
  const child = path.join(directory, 'server.mjs');
  fs.writeFileSync(
    child,
    `${imports}
import {Server} from ${ipc};
import {Emitter} from ${event};
const server=new Server('elytra-fork-component');
const emitter=new Emitter();
server.registerChannel('controlled',{
 call(_context,command,arg){
  if(command==='echo')return Promise.resolve(arg);
  if(command==='notify'){emitter.fire(arg);return Promise.resolve();}
  if(command==='fail')return Promise.reject(Error('Controlled IPC error'));
  if(command==='shutdown'){setTimeout(()=>process.disconnect(),20);return Promise.resolve();}
  return Promise.reject(Error('Unexpected IPC command'));
 },
 listen(_context,name){if(name!=='value')throw Error('Unexpected IPC event');return emitter.event;}
});
const deadline=setTimeout(()=>{console.error('Code-OSS fork server timed out');process.exit(1);},8000);
deadline.unref();
process.once('disconnect',()=>emitter.dispose());
`
  );
  const parent = path.join(directory, 'client.mjs');
  fs.writeFileSync(
    parent,
    `${imports}
import {Client} from ${ipc};
import {VSBuffer} from ${buffer};
const check=(value,message)=>{if(!value)throw Error(message);};
const client=new Client(${JSON.stringify(child)},{serverName:'Elytra actual fork',freshExecArgv:true,useQueue:true});
const deadline=setTimeout(()=>{client.dispose();throw Error('Code-OSS fork client timed out');},7000);
try{
 const exited=new Promise(resolve=>client.onDidProcessExit(resolve));
 const channel=client.getChannel('controlled');
 let event;
 const received=new Promise(resolve=>event=resolve);
 const listener=channel.listen('value')(event);
 const bytes=Uint8Array.from({length:512*1024},(_,index)=>index%251);
 const payload={text:'Grüße 日本語 🪲',nested:[null,true,42]};
 const roundtrip=await channel.call('echo',payload);
 check(roundtrip.text===payload.text&&JSON.stringify(roundtrip.nested)===JSON.stringify(payload.nested),'Actual ipc.cp structured response');
 const binary=await channel.call('echo',VSBuffer.wrap(bytes));
 check(binary instanceof VSBuffer&&binary.byteLength===bytes.length,'Actual Code-OSS binary response type');
 for(let index=0;index<bytes.length;index++)check(binary.buffer[index]===bytes[index],'Actual base64 transport bytes');
 const replies=await Promise.all(Array.from({length:8},(_,index)=>channel.call('echo',{index})));
 check(replies.every((value,index)=>value.index===index),'Actual Code-OSS concurrent request correlation');
 let failed;
 try{await channel.call('fail');}catch(error){failed=error.message;}
 check(failed==='Controlled IPC error','Actual ipc.cp remote error');
 await channel.call('notify',{nested:{value:'actual-event'}});
 check((await received).nested.value==='actual-event','Actual Code-OSS event subscription');
 await channel.call('shutdown');
 const exit=await exited;
 check(exit.code===0&&exit.signal===null,'Actual Code-OSS child exit cleanup');
 client.dispose();
 listener.dispose();
 console.log('PASS actual pinned Code-OSS ipc.cp Client/Server: binary/base64, concurrent calls, remote errors, events and natural child exit');
}finally{clearTimeout(deadline);client.dispose();}
`
  );
  let reference;
  for (const [name, binary] of [
    ['Node', process.execPath],
    ...(referenceOnly ? [] : [['Mite', path.resolve(miteArg)]])
  ]) {
    const result = spawnSync(binary, [parent], {
      encoding: 'utf8',
      timeout: 12000,
      env: { ...process.env, NODE_OPTIONS: '' }
    });
    if (
      result.status !== 0 ||
      result.stderr ||
      !result.stdout.startsWith('PASS actual pinned Code-OSS ipc.cp')
    ) {
      throw Error(JSON.stringify({ name, ...result }));
    }
    if (reference !== undefined && reference !== result.stdout) {
      throw Error('Code-OSS Node/Mite fork output differs');
    }
    reference = result.stdout;
    process.stdout.write(name + ': ' + result.stdout);
  }
  console.log(
    'PASS ' +
      verified.size +
      ' compiled dependencies verified; component acceptance only, no workbench claim'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
