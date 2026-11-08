/** Execute the actual pinned Code-OSS MessagePort Protocol on public desktop ports. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import { transformSync } from '../runtime-js/node_modules/esbuild/lib/main.js';

const root = fileURLToPath(new URL('../../', import.meta.url));
const referenceOnly = process.argv.includes('--reference-only');
const [treeArg, fireflyArg, electronArg] = process.argv
  .slice(2)
  .filter((value) => value !== '--reference-only');
if (referenceOnly && !electronArg) {
  throw Error('--reference-only requires the Electron binary');
}
if (!treeArg || !fireflyArg) {
  throw Error(
    'Require compiled Code-OSS tree and Firefly binary; optional Electron reference binary'
  );
}
const tree = path.resolve(treeArg);
const firefly = path.resolve(fireflyArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/ipc-mp-node.ts'));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

if (hash(source) !== pin.files['ipc-mp-node.ts']?.sha256) {
  throw Error('MessagePort protocol source drift');
}
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));
if (report.kind !== 'diagnostic-source-tree' || report.adaptations?.revision !== pin.revision) {
  throw Error('Wrong Code-OSS compiled baseline');
}

function imported(relative, names) {
  const output = `src/vs/base/${relative}.js`;
  const record = report.inputs.find((item) => item.output === output);
  if (!record || hash(fs.readFileSync(path.join(tree, output))) !== record.outputSha256) {
    throw Error(`Compiled MessagePort dependency drift: ${output}`);
  }
  return `import { ${names} } from ${JSON.stringify(pathToFileURL(path.join(tree, output)).href)};\n`;
}

const original = source.toString();
const notice = original.slice(0, original.indexOf('\nimport '));
const protocolStart = 'class Protocol implements IMessagePassingProtocol {';
const protocolEnd = '\nexport interface IClientConnectionFilter {';
const onceStart = 'export function once(';
if (
  original.split(protocolStart).length !== 2 ||
  original.split(protocolEnd).length !== 2 ||
  original.split(onceStart).length !== 2
) {
  throw Error('Pinned MessagePort protocol seams changed');
}
const protocol = original.slice(original.indexOf(protocolStart), original.indexOf(protocolEnd));
const once = original.slice(original.indexOf(onceStart));
const services = imported('common/buffer', 'VSBuffer') + imported('common/event', 'Event');
const prefix = `import ${JSON.stringify(pathToFileURL(path.join(tree, 'src/nls.messages.js')).href)};\n`;

function driver(module) {
  return `${notice}\nimport { app, MessageChannelMain } from ${JSON.stringify(module)};
${prefix}${services}
${protocol}
${once}
const check=(condition,message)=>{if(!condition)throw Error(message);};
const delay=milliseconds=>new Promise(resolve=>setTimeout(resolve,milliseconds));
app.whenReady().then(async()=>{
 const channels=[];const disposables=[];
 const deadline=setTimeout(()=>{console.error('Code-OSS port component timed out');app.exit(1);},5000);
 try {
  const channel=new MessageChannelMain();channels.push(channel);
  const left=new Protocol(channel.port1);const right=new Protocol(channel.port2);
  const received=[];const echoed=[];
  disposables.push(right.onMessage(message=>{received.push(Array.from(message.buffer));right.send(message);}));
  disposables.push(left.onMessage(message=>echoed.push(Array.from(message.buffer))));
  const payload=new Uint8Array([0,255,128,1,2,3]);
  left.send(VSBuffer.wrap(payload.subarray(1,5)));payload.fill(77);
  left.send(VSBuffer.alloc(0));
  left.send(VSBuffer.fromString('Unicode ä 😀'));
  const expected=[[255,128,1,2],[],Array.from(VSBuffer.fromString('Unicode ä 😀').buffer)];
  for(let attempt=0;attempt<100&&echoed.length<3;attempt++)await delay(5);
  check(JSON.stringify(received)===JSON.stringify(expected),'Actual Protocol frame conversion/FIFO/copy failed');
  check(JSON.stringify(echoed)===JSON.stringify(expected),'Actual Protocol binary echo failed');
  let disconnected=0;channel.port2.on('close',()=>disconnected++);
  left.disconnect();
  for(let attempt=0;attempt<100&&!disconnected;attempt++)await delay(5);
  check(disconnected===1,'Actual Protocol disconnect did not reach peer');
  // Exercise the unchanged exact-match once helper after real ownership handoff.
  const carrier=new MessageChannelMain();const connection=new MessageChannelMain();channels.push(carrier,connection);
  const handed=new Promise(resolve=>carrier.port2.once('message',event=>resolve(event.ports[0])));
  carrier.port2.start();
  carrier.port1.postMessage('handoff',[connection.port2]);
  const accepted=await handed;
  let matched=0;once(accepted,'ready',()=>matched++);accepted.start();
  connection.port1.postMessage('ignore');connection.port1.postMessage('ready');connection.port1.postMessage('ready');
  for(let attempt=0;attempt<100&&!matched;attempt++)await delay(5);
  await delay(10);
  check(matched===1,'Actual once helper failed to remove its matching listener');
  accepted.close();
  console.log('PASS actual Code-OSS MessagePort Protocol: binary frames/FIFO/snapshot/echo/disconnect and once helper after port transfer');
 } finally {
  clearTimeout(deadline);
  for(const disposable of disposables)disposable.dispose();
  for(const channel of channels){channel.port1.close();channel.port2.close();}
 }
 app.exit(0);
}).catch(error=>{console.error(error);app.exit(1);});`;
}

const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-message-port-'));
try {
  for (const [binary, module] of [
    ...(electronArg ? [[path.resolve(electronArg), 'electron']] : []),
    ...(referenceOnly ? [] : [[firefly, 'firefly']])
  ]) {
    const filename = path.join(directory, `${module}.mjs`);
    fs.writeFileSync(
      filename,
      transformSync(driver(module), { loader: 'ts', format: 'esm', target: 'es2024' }).code
    );
    const result = spawnSync(binary, [filename], { encoding: 'utf8', timeout: 15000 });
    if (
      result.status !== 0 ||
      !result.stdout.includes('PASS actual Code-OSS MessagePort Protocol')
    ) {
      throw Error(JSON.stringify(result));
    }
    process.stdout.write(`${module}: ${result.stdout}`);
  }
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
