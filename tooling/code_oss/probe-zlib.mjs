/** Execute the actual pinned Code-OSS compression wrappers with real compiled base services. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import { transformSync } from '../runtime-js/node_modules/esbuild/lib/main.js';
const root = fileURLToPath(new URL('../../', import.meta.url));
const [tree, mite] = process.argv.slice(2).map((value) => path.resolve(value));
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/ipc-net-node.ts'));
if (createHash('sha256').update(source).digest('hex') !== pin.files['ipc-net-node.ts'].sha256) {
  throw Error('IPC source drift');
}
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

function imported(relative, names) {
  const output = 'src/vs/base/' + relative + '.js';
  const record = report.inputs.find((input) => input.output === output);
  if (
    !record ||
    createHash('sha256')
      .update(fs.readFileSync(path.join(tree, output)))
      .digest('hex') !== record.outputSha256
  ) {
    throw Error('Compiled service drift: ' + output);
  }
  return `import {${names}} from ${JSON.stringify(pathToFileURL(path.join(tree, output)).href)};\n`;
}

const text = source.toString();
const classes = text.slice(
  text.indexOf('class ZlibInflateStream extends Disposable'),
  text.indexOf('// Read this before')
);
if (
  !classes.startsWith('class ZlibInflateStream') ||
  !classes.includes('class ZlibDeflateStream')
) {
  throw Error('IPC wrapper seams changed');
}
const driver = `import {createInflateRaw,createDeflateRaw} from 'node:zlib';
${imported('common/buffer', 'VSBuffer')}${imported('common/event', 'Emitter')}${imported('common/lifecycle', 'Disposable')}${imported('parts/ipc/common/ipc.net', 'SocketDiagnosticsEventType')}
${classes}
const events=[];const tracer={traceSocketEvent(...args){events.push(args);}};
const encoder=new ZlibDeflateStream(tracer,{windowBits:15});
const decoder=new ZlibInflateStream(tracer,true,null,{windowBits:15});
let failed;
encoder.onError(error=>{failed=error;});decoder.onError(error=>{failed=error;});
for(const text of ['hello'.repeat(1000),'hello'.repeat(2000),'Unicode ü 😀']){
 encoder.write(VSBuffer.fromString(text));
 const wire=await new Promise(resolve=>encoder.flush(resolve));
 decoder.write(wire);decoder.write(VSBuffer.fromByteArray([0,0,255,255]));
 const decoded=await new Promise(resolve=>decoder.flush(resolve));
 if(decoded.toString()!==text||failed)throw failed??Error('Actual wrapper roundtrip failed');
}
if(!events.length||!decoder.recordedInflateBytes.byteLength)throw Error('Missing real wrapper state');
encoder.dispose();decoder.dispose();console.log('PASS actual Code-OSS ZlibDeflateStream/ZlibInflateStream messages and context takeover');`;
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-zlib-'));
try {
  const filename = path.join(directory, 'probe.mjs');
  fs.writeFileSync(
    filename,
    transformSync(driver, { loader: 'ts', format: 'esm', target: 'es2024' }).code
  );
  for (const binary of [process.execPath, mite]) {
    const result = spawnSync(binary, [filename], { encoding: 'utf8', timeout: 20000 });
    if (result.status !== 0 || !result.stdout.includes('PASS actual Code-OSS')) {
      throw Error(JSON.stringify(result));
    }
    process.stdout.write(result.stdout);
  }
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
