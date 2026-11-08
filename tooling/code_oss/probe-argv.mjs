/** Run the actual compiled Code-OSS CLI validator; no replacement parser/assertion. */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
const root = fileURLToPath(new URL('../../', import.meta.url));
const [tree, mite] = process.argv.slice(2).map((value) => path.resolve(value));
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const record = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));
const original = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/argvHelper.ts'));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const output = 'src/vs/platform/environment/node/argvHelper.js';
const input = record.inputs.find((item) => item.output === output);
if (
  hash(original) !== pin.files['argvHelper.ts'].sha256 ||
  input?.sourceSha256 !== hash(original) ||
  input.outputSha256 !== hash(fs.readFileSync(path.join(tree, output)))
) {
  throw Error('CLI source/output drift');
}
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-argv-'));
try {
  const driver = path.join(directory, 'driver.mjs');
  fs.writeFileSync(
    driver,
    `import ${JSON.stringify(pathToFileURL(path.join(tree, 'src/nls.messages.js')).href)};
const {parseMainProcessArgv,parseCLIProcessArgv}=await import(${JSON.stringify(pathToFileURL(path.join(tree, output)).href)});
for(const [parse,argv] of [[parseMainProcessArgv,['code','--goto','file.ts:2:3']],[parseCLIProcessArgv,['code','cli','--goto','file.ts:2:3']]]){
 const result=parse(argv);if(!result.goto||!result._.includes('file.ts:2:3'))throw Error('Valid CLI refused');
 let error;try{parse([...argv.slice(0,-1),'file.ts:not-a-line']);}catch(value){error=value;}
 if(error?.code!=='ERR_ASSERTION'||!error.message.includes('Arguments in'))throw Error('Invalid CLI did not assert');
}
console.log('PASS actual Code-OSS main/CLI goto validation, including AssertionError refusal');`
  );
  for (const binary of [process.execPath, mite]) {
    const result = spawnSync(binary, [driver], { encoding: 'utf8', timeout: 20000 });
    if (result.status !== 0 || !result.stdout.includes('PASS actual Code-OSS')) {
      throw Error(JSON.stringify(result));
    }
    process.stdout.write(result.stdout);
  }
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
