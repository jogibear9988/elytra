/** Run the pinned Code-OSS dialog option consumer against actual Node/Mite modules (no UI). */
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../', import.meta.url));
const [treeArg, miteArg] = process.argv.slice(2);
if (!treeArg || !miteArg) {
  throw Error('Require compiled Code-OSS tree and Mite CLI');
}
const tree = path.resolve(treeArg);
const pin = JSON.parse(fs.readFileSync(path.join(root, 'vendor/code-oss-tests/manifest.json')));
const report = JSON.parse(fs.readFileSync(path.join(tree, 'ELYTRA-COMPILE.json')));

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

const source = fs.readFileSync(path.join(root, 'vendor/code-oss-tests/dialogMainUtils.ts'));
const output = 'src/vs/platform/dialogs/electron-main/dialogMainUtils.js';
const records = new Map(
  [...report.inputs, ...(report.generated ?? [])].map((item) => [item.output, item])
);
if (
  report.kind !== 'diagnostic-source-tree' ||
  report.adaptations?.revision !== pin.revision ||
  hash(source) !== pin.files['dialogMainUtils.ts']?.sha256 ||
  records.get(output)?.sourceSha256 !== hash(source)
) {
  throw Error('Pinned Code-OSS massageMessageBoxOptions source/compiled revision drift');
}
const verified = new Set();

function verify(file) {
  if (verified.has(file)) {
    return;
  }
  const bytes = fs.readFileSync(path.join(tree, file));
  if (hash(bytes) !== records.get(file)?.outputSha256) {
    throw Error('Compiled massageMessageBoxOptions dependency drift: ' + file);
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

const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'elytra-code-dialog-'));
try {
  const script = path.join(directory, 'dialog-options.mjs');
  fs.writeFileSync(
    script,
    `
import ${url('src/nls.messages.js')};
import {massageMessageBoxOptions} from ${url(output)};
const check=(value,message)=>{if(!value)throw Error(message);};
const product={nameLong:'Owned dialog component'};
const input={message:'Grüße 日本語',detail:'Details',buttons:['Save','Discard','Cancel'],cancelId:2,checkboxLabel:'Remember',checkboxChecked:true};
const before=JSON.stringify(input);
const {options,buttonIndeces}=massageMessageBoxOptions(input,product);
check(JSON.stringify(input)===before,'Actual consumer must not mutate original options');
check(options.title===product.nameLong&&options.noLink===true,'Product title and no-link policy');
check(options.message===input.message&&options.detail==='Details'&&options.checkboxChecked===true,'Native message/checkbox information preserved');
check(buttonIndeces.length===3&&new Set(buttonIndeces).size===3&&buttonIndeces.every(value=>Number.isInteger(value)&&value>=0&&value<3),'Actual button index mapping is a permutation');
check(buttonIndeces[options.cancelId]===2,'Platform cancel mapping points back to original cancel button');
check(buttonIndeces[options.defaultId]===0,'Platform default mapping points back to preferred original first button');
check(options.buttons[options.cancelId]==='Cancel','Platform-correct cancel button label');
options.buttons[0]='Mutated copy';
check(input.buttons[0]==='Save','Consumer returns independently owned button array');
const empty=massageMessageBoxOptions({message:'Empty',buttons:[]},product);
check(empty.options.buttons.length===0&&empty.buttonIndeces.length===0,'Empty button input preserved for genuine backend default handling');
console.log('PASS actual pinned Code-OSS massageMessageBoxOptions immutable options, platform button mapping, title and checkbox policy; no dialogs shown');
`
  );
  const results = [process.execPath, path.resolve(miteArg)].map((binary) =>
    spawnSync(binary, [script], { encoding: 'utf8', timeout: 8000 })
  );
  for (const result of results) {
    if (
      result.status !== 0 ||
      result.stderr ||
      !result.stdout.startsWith('PASS actual pinned Code-OSS massageMessageBoxOptions')
    ) {
      throw Error(JSON.stringify(result));
    }
  }
  if (results[0].stdout !== results[1].stdout) {
    throw Error('Node/Mite actual dialog consumer observations differ');
  }
  process.stdout.write(results[0].stdout);
  console.log(
    'PASS ' +
      verified.size +
      ' compiled dependencies verified; pure dialog option consumer only, full DialogMainService/OS delivery unaccepted'
  );
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}
