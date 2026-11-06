/** Format maintained web/JS sources and keep definitions visually separated. */
import { readFile, writeFile } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import * as prettier from 'prettier';
import { parse } from 'acorn';

const root = path.resolve(import.meta.dirname, '../..');
const check = process.argv.includes('--check');
const requested = process.argv.slice(2).filter((value) => value !== '--check');
const files = requested.length
  ? requested
  : execFileSync('git', ['ls-files', '--cached', '--others', '--exclude-standard', '-z'], {
      cwd: root
    })
      .toString()
      .split('\0')
      .filter(Boolean);
const ignored =
  /^(?:vendor|build)\/|\/node_modules\/|\/generated\/|^mite\/tests\/|^firefly\/tests\/|package-lock\.json$/;
const extensions = new Set(['.js', '.mjs', '.cjs', '.html', '.css', '.json', '.yml', '.yaml']);
const config = JSON.parse(await readFile(path.join(root, '.prettierrc.json'), 'utf8'));

function syntax(source, file) {
  const common = { ecmaVersion: 'latest', locations: true, allowHashBang: true };
  if (file.endsWith('.mjs')) return parse(source, { ...common, sourceType: 'module' });
  try {
    return parse(source, { ...common, sourceType: 'module' });
  } catch {
    return parse(source, { ...common, sourceType: 'script', allowReturnOutsideFunction: true });
  }
}

function isFunction(node) {
  return (
    node &&
    ['FunctionDeclaration', 'FunctionExpression', 'ArrowFunctionExpression'].includes(node.type)
  );
}

function definition(node) {
  if (isFunction(node) || node?.type === 'MethodDefinition') return true;
  if (['ExportDefaultDeclaration', 'ExportNamedDeclaration'].includes(node?.type))
    return definition(node.declaration);
  if (node?.type === 'ExpressionStatement') return definition(node.expression);
  if (node?.type === 'AssignmentExpression') return isFunction(node.right);
  if (node?.type === 'CallExpression') return node.arguments.some(isFunction);
  if (node?.type === 'VariableDeclaration')
    return node.declarations.some((value) => isFunction(value.init));
  return node?.type === 'Property' && isFunction(node.value);
}

function separateDefinitions(source, file) {
  const ast = syntax(source, file);
  const after = new Set();

  function visit(node) {
    if (!node || typeof node !== 'object') return;
    const siblings = ['Program', 'BlockStatement', 'ClassBody'].includes(node.type)
      ? node.body
      : node.type === 'ObjectExpression'
        ? node.properties
        : null;
    if (Array.isArray(siblings)) {
      for (let index = 0; index + 1 < siblings.length; index++) {
        const left = siblings[index],
          right = siblings[index + 1];
        if (
          (definition(left) || definition(right)) &&
          right.loc.start.line === left.loc.end.line + 1
        )
          after.add(left.loc.end.line);
      }
    }
    for (const [key, value] of Object.entries(node)) {
      if (['loc', 'start', 'end'].includes(key)) continue;
      if (Array.isArray(value)) value.forEach(visit);
      else visit(value);
    }
  }

  visit(ast);
  return source
    .split('\n')
    .map((line, index) => line + (after.has(index + 1) ? '\n' : ''))
    .join('\n');
}

let checked = 0,
  changed = 0;
for (const file of [...new Set(files)].sort()) {
  const extension = path.extname(file);
  if (ignored.test(file) || !extensions.has(extension)) continue;
  const absolute = path.join(root, file);
  const original = await readFile(absolute, 'utf8');
  let formatted = await prettier.format(original, { ...config, filepath: absolute });
  if (['.js', '.mjs', '.cjs'].includes(extension)) formatted = separateDefinitions(formatted, file);
  checked++;
  if (formatted !== original) {
    changed++;
    if (!check) await writeFile(absolute, formatted);
    else if (changed <= 10) process.stderr.write(`Needs formatting: ${file}\n`);
  }
}
process.stdout.write(
  `${check ? 'Checked' : 'Formatted'} ${checked} maintained JS/web/config files; ${changed} ${check ? 'need changes' : 'updated'}.\n`
);
if (check && changed) process.exitCode = 1;
