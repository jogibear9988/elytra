/** Compile authored ESM factories into the synchronous LibJS embedding ABI. */
import * as esbuild from 'esbuild';

export async function compileFactory(repository, entry) {
  const result = await esbuild.build({
    absWorkingDir: repository,
    entryPoints: [entry],
    bundle: true,
    format: 'esm',
    platform: 'neutral',
    target: 'es2022',
    write: false,
    metafile: true,
    legalComments: 'inline',
    charset: 'utf8',
    logLevel: 'silent'
  });
  if (result.warnings.length) {
    throw Error(JSON.stringify(result.warnings));
  }
  if (Object.values(result.metafile.outputs).some((output) => output.imports.length)) {
    throw Error('Embedded factories must bundle their private imports');
  }
  const code = result.outputFiles[0].text;
  // This is the pinned compiler's single-default-export representation. Reject
  // format drift instead of accepting partial ESM or evaluating an import later.
  const exported = code.match(/\nexport \{\n  ([$A-Z_a-z][$\w]*) as default\n\};\n$/);
  if (!exported) {
    throw Error('Expected exactly one named default factory export');
  }
  const body = code.slice(0, exported.index);
  const source =
    `/* Generated from ${entry}; edit the ESM source. */\n` +
    `(function () {\n'use strict';\n${body}\nreturn ${exported[1]};\n})()\n`;
  return { source, inputs: Object.keys(result.metafile.inputs).sort() };
}
