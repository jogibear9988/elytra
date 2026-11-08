/** Build-only indexed English NLS using Code-OSS's pinned source analyzer. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { transformSync } from '../runtime-js/node_modules/esbuild/lib/main.js';
import { parseExpressionAt } from './node_modules/acorn/dist/acorn.mjs';
import ts from './node_modules/typescript/lib/typescript.js';

const repository = fileURLToPath(new URL('../../', import.meta.url));
const pin = JSON.parse(
  fs.readFileSync(path.join(repository, 'vendor/code-oss-tests/manifest.json'))
);

const digest = (data) => createHash('sha256').update(data).digest('hex');

/** Refuse altered reference tools before executing any of their build code. */
async function reference(name) {
  const bytes = fs.readFileSync(path.join(repository, 'vendor/code-oss-tests', name));
  if (digest(bytes) !== pin.files[name].sha256) {
    throw new Error(`NLS reference drift: ${name}`);
  }
  let code = transformSync(bytes.toString('utf8'), {
    loader: 'ts',
    format: 'esm',
    target: 'es2024'
  }).code;
  if (name === 'nls-analysis.ts') {
    const seam = 'import * as ts from "typescript";';
    if (code.split(seam).length !== 2) {
      throw new Error('NLS TypeScript import changed');
    }
    code = code.replace(
      seam,
      `import ts from ${JSON.stringify(pathToFileURL(path.join(repository, 'tooling/code_oss/node_modules/typescript/lib/typescript.js')).href)};`
    );
  }
  return import(`data:text/javascript;base64,${Buffer.from(code).toString('base64')}`);
}

/** Decode only literal messages/keys; do not eval application expressions. */
export function literal(expression, key = false) {
  const node = parseExpressionAt(expression, 0, { ecmaVersion: 'latest' });
  if (expression.slice(node.end).trim()) {
    throw new Error('Trailing NLS expression');
  }
  if (node.type === 'Literal' && typeof node.value === 'string') {
    return node.value;
  }
  if (node.type === 'TemplateLiteral' && node.expressions.length === 0) {
    return node.quasis[0].value.cooked;
  }
  if (key && node.type === 'ObjectExpression') {
    const result = {};
    for (const property of node.properties) {
      if (
        property.type !== 'Property' ||
        property.computed ||
        property.method ||
        property.kind !== 'init'
      ) {
        break;
      }
      const name = property.key.name ?? property.key.value;
      if (!['key', 'comment'].includes(name) || Object.hasOwn(result, name)) {
        break;
      }
      const value = property.value;
      if (name === 'comment' && value.type === 'ArrayExpression') {
        result.comment = value.elements.map((item) => {
          if (!item) {
            throw new Error('Sparse NLS translator comments');
          }
          return literal(expression.slice(item.start, item.end));
        });
      } else if (name === 'key') {
        result.key = literal(expression.slice(value.start, value.end));
      } else {
        break;
      }
    }
    if (typeof result.key === 'string' && Object.keys(result).length === node.properties.length) {
      return result;
    }
  }
  throw new Error(`Nonliteral NLS ${key ? 'key' : 'message'}: ${expression.slice(0, 120)}`);
}

export async function createNLS() {
  const compilerPackage = JSON.parse(
    fs.readFileSync(new URL('./node_modules/typescript/package.json', import.meta.url))
  );
  // Upstream's typescript6 6.0.2 package reports compiler version 6.0.3.
  if (
    compilerPackage.name !== '@typescript/typescript6' ||
    compilerPackage.version !== '6.0.2' ||
    ts.version !== '6.0.3'
  ) {
    throw new Error('Require pinned @typescript/typescript6 6.0.2 build tool');
  }
  const { analyzeLocalizeCalls } = await reference('nls-analysis.ts');
  const { serializeNlsData } = await reference('nlsMessages.ts');
  const builderSha256 = digest(fs.readFileSync(fileURLToPath(import.meta.url)));
  const messages = [];
  const modules = [];
  const metadata = { keys: Object.create(null), messages: Object.create(null) };
  let previous;
  return {
    /** Call in output-module order, before esbuild; preserve all English fallbacks. */
    patch(name, source) {
      if (previous !== undefined && name <= previous) {
        throw new Error('NLS inputs must be unique and sorted');
      }
      previous = name;
      if (!/\/nls(?:\.js)?['"]/.test(source)) {
        return source;
      }
      const calls = [
        ...analyzeLocalizeCalls(source, 'localize'),
        ...analyzeLocalizeCalls(source, 'localize2')
      ];
      if (!calls.length) {
        return source;
      }
      const model = ts.createSourceFile(name, source, ts.ScriptTarget.Latest, true);
      const patches = [];
      const keys = [];
      const values = [];
      for (const call of calls) {
        const key = literal(call.key, true);
        const value = literal(call.value);
        keys.push(key);
        values.push(value);
        const start = model.getPositionOfLineAndCharacter(
          call.keySpan.start.line,
          call.keySpan.start.character
        );
        const end = model.getPositionOfLineAndCharacter(
          call.keySpan.end.line,
          call.keySpan.end.character
        );
        if (source.slice(start, end) !== call.key) {
          throw new Error(`NLS span mismatch: ${name}`);
        }
        patches.push({ start, end, value: String(messages.length) });
        messages.push(value);
      }
      // Upstream assigns localize indices before localize2 indices, then applies
      // replacements in reverse source order; do the same without sourcemap loss.
      patches.sort((a, b) => b.start - a.start);

      let boundary = source.length;
      for (const patch of patches) {
        if (patch.end > boundary) {
          throw new Error(`Overlapping NLS spans: ${name}`);
        }
        source = source.slice(0, patch.start) + patch.value + source.slice(patch.end);
        boundary = patch.start;
      }
      const id = name.replace(/^src\//, '').replace(/\.(?:ts|js)$/, '');
      modules.push([id, keys.map((key) => (typeof key === 'string' ? key : key.key))]);
      metadata.keys[id] = keys;
      metadata.messages[id] = values;
      return source;
    },

    artifacts() {
      return new Map([
        ['src/nls.messages.json', Buffer.from(JSON.stringify(messages))],
        ['src/nls.keys.json', Buffer.from(JSON.stringify(modules))],
        ['src/nls.metadata.json', Buffer.from(JSON.stringify(metadata, null, '\t'))],
        [
          'src/nls.messages.js',
          Buffer.from(
            `/* Copyright (c) Microsoft Corporation. MIT License; see LICENSE.txt. */\nglobalThis._VSCODE_NLS_MESSAGES=${serializeNlsData(messages)};\n`
          )
        ]
      ]);
    },

    report() {
      return {
        kind: 'indexed-english-preserve-fallbacks',
        builderSha256,
        messages: messages.length,
        modules: modules.length,
        analyzerSha256: pin.files['nls-analysis.ts'].sha256,
        serializerSha256: pin.files['nlsMessages.ts'].sha256,
        typescriptPackage: '@typescript/typescript6@6.0.2',
        typescript: ts.version,
        localizedLanguagePacks: false
      };
    }
  };
}
