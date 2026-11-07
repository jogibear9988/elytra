/** Compile the exact pinned parseCLIArgs function for a scoped, non-GUI runtime test. */
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { transformSync, version } from '../runtime-js/node_modules/esbuild/lib/main.js';
import { parse } from './node_modules/acorn/dist/acorn.mjs';

const [source, packageEntry, output] = process.argv.slice(2);
const manifest = JSON.parse(
  fs.readFileSync(new URL('../../vendor/code-oss-tests/manifest.json', import.meta.url))
);
const bytes = fs.readFileSync(source);
if (
  version !== '0.25.12' ||
  createHash('sha256').update(bytes).digest('hex') !== manifest.files['mainImpl.ts'].sha256
) {
  throw new Error('CLI extraction requires exact pinned Code-OSS source and compiler');
}
const compiled = transformSync(bytes.toString('utf8'), {
  loader: 'ts',
  format: 'esm',
  target: 'es2024'
}).code;
const declarations = parse(compiled, { sourceType: 'module', ecmaVersion: 'latest' }).body.filter(
  (node) => node.type === 'FunctionDeclaration' && node.id.name === 'parseCLIArgs'
);
if (declarations.length !== 1 || declarations[0].params.length) {
  throw new Error('Code-OSS CLI function seam changed');
}
const declaration = compiled.slice(declarations[0].start, declarations[0].end);
// No fake desktop/services: this test invokes only the real compiled subroutine.
fs.writeFileSync(
  output,
  `/** Code-OSS MIT source; see adjacent LICENSE.txt. */\nimport minimist from ${JSON.stringify(pathToFileURL(packageEntry).href)};\nexport ${declaration}\n`
);
