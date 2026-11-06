/** Reproduce embedded builtin adapters and portable dependency bundles. */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import * as esbuild from 'esbuild';
import { compileFactory } from './factories.mjs';
const filename = fileURLToPath(import.meta.url);
const root = path.dirname(filename);
const repository = path.resolve(root, '../..');
const destination = path.resolve(root, '../../mite/modules/generated');
const lock = JSON.parse(fs.readFileSync(path.join(root, 'package-lock.json'), 'utf8'));
const check = process.argv.includes('--check');

const digest = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');

const external = ['node:buffer', 'node:events', 'node:process', 'node:string_decoder'];
const alias = { buffer: 'node:buffer', events: 'node:events' };

function relative(file) {
  return path.relative(root, file).split(path.sep).join('/');
}

function metadata(file) {
  if (!relative(file).startsWith('node_modules/')) {
    return null;
  }
  let directory = path.dirname(file);
  while (directory !== root && !fs.existsSync(path.join(directory, 'package.json'))) {
    directory = path.dirname(directory);
  }
  if (directory === root) {
    return null;
  }
  const pkg = JSON.parse(fs.readFileSync(path.join(directory, 'package.json'), 'utf8'));
  const pin = lock.packages[relative(directory)];
  if (!pin || pin.version !== pkg.version || !pin.integrity) {
    throw Error(`Unpinned package: ${pkg.name}`);
  }
  const licenses = fs
    .readdirSync(directory)
    .filter((name) => /^(license|copying)([.-].*)?$/i.test(name))
    .sort();
  if (!licenses.length) {
    throw Error(`Missing license: ${pkg.name}`);
  }
  const notices = licenses.map((name) => ({
    path: relative(path.join(directory, name)),
    text: fs.readFileSync(path.join(directory, name), 'utf8')
  }));
  return {
    name: pkg.name,
    version: pkg.version,
    license: pkg.license,
    integrity: pin.integrity,
    notices
  };
}

function save(name, text, manifest) {
  const file = path.join(destination, name);
  if (check) {
    if (!fs.existsSync(file) || fs.readFileSync(file, 'utf8') !== text) {
      throw Error(`Generated output is stale: ${name}`);
    }
  } else if (!fs.existsSync(file) || fs.readFileSync(file, 'utf8') !== text) {
    fs.writeFileSync(file, text);
  }
  manifest.files[name] = digest(text);
}

(async () => {
  const manifest = {
    generator: `esbuild ${esbuild.version}`,
    builderSha256: digest(fs.readFileSync(filename)),
    lockSha256: digest(fs.readFileSync(path.join(root, 'package-lock.json'))),
    files: {},
    bundles: {},
    sources: {}
  };
  if (!check) {
    fs.mkdirSync(destination, { recursive: true });
  }
  const pinned = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'))
    .devDependencies.esbuild;
  if (esbuild.version !== pinned) {
    throw Error('Install the pinned runtime-js lockfile');
  }
  for (const source of ['tooling/runtime-js/factories.mjs', 'tooling/runtime-js/modules.json']) {
    manifest.sources[source] = digest(fs.readFileSync(path.join(repository, source)));
  }
  const modules = JSON.parse(fs.readFileSync(path.join(root, 'modules.json'), 'utf8'));
  for (const [name, entry] of Object.entries(modules)) {
    const compiled = await compileFactory(repository, entry);
    const output = name === 'bootstrap' ? compiled.source.trimEnd() + '();\n' : compiled.source;
    save(name + '.js', output, manifest);
    for (const input of compiled.inputs) {
      if (!input.startsWith('mite/modules/') && !input.startsWith('mite/runtime/bootstrap/')) {
        throw Error('Unexpected builtin input: ' + input);
      }
      manifest.sources[input] = digest(fs.readFileSync(path.join(repository, input)));
    }
  }
  for (const name of ['string-decoder', 'stream', 'web-globals']) {
    const result = await esbuild.build({
      absWorkingDir: root,
      entryPoints: [`${name}-entry.mjs`],
      outfile: `${name}.js`,
      bundle: true,
      format: 'cjs',
      platform: 'node',
      target: 'es2022',
      write: false,
      metafile: true,
      legalComments: 'inline',
      charset: 'utf8',
      external,
      alias,
      logLevel: 'silent',
      plugins: [
        {
          name: 'mite-builtins',

          setup(build) {
            build.onResolve({ filter: new RegExp('^(process|string_decoder)/$') }, (args) => ({
              path: 'node:' + args.path.slice(0, -1),
              external: true
            }));
          }
        }
      ]
    });
    if (result.warnings.length) {
      throw Error(JSON.stringify(result.warnings));
    }
    const output = Object.values(result.metafile.outputs)[0];
    const imports = [...new Set(output.imports.map((item) => item.path))].sort();
    if (output.imports.some((item) => !item.external || !external.includes(item.path))) {
      throw Error(`Unexpected import in ${name}`);
    }
    if (name === 'web-globals' && imports.length) {
      throw Error('Initial event/abort globals cannot import runtime builtins');
    }
    const packages = new Map();
    const sources = Object.keys(result.metafile.inputs)
      .sort()
      .map((input) => {
        const file = path.resolve(root, input);
        const repositoryPath = path.relative(repository, file).split(path.sep).join('/');
        if (!input.startsWith('node_modules/') && !path.relative(root, file).startsWith('..')) {
          manifest.sources[repositoryPath] = digest(fs.readFileSync(file));
        } else if (repositoryPath.startsWith('mite/runtime/bootstrap/')) {
          manifest.sources[repositoryPath] = digest(fs.readFileSync(file));
        } else if (!input.startsWith('node_modules/')) {
          throw Error('Unexpected portable adapter source: ' + input);
        }
        // Authored bootstrap inputs are already recorded with repository-relative
        // names above. Dependency-bundle paths stay within the build package.
        if (repositoryPath.startsWith('mite/runtime/bootstrap/')) {
          return null;
        }
        const pkg = metadata(file);
        if (pkg) {
          packages.set(pkg.name, pkg);
        }
        return { path: input, sha256: digest(fs.readFileSync(file)) };
      })
      .filter(Boolean);
    const records = [...packages.values()].sort((a, b) => a.name.localeCompare(b.name, 'en'));
    const notices = records
      .map(
        (pkg) =>
          `${pkg.name} ${pkg.version} (${pkg.license})\n` +
          pkg.notices.map((item) => item.text).join('\n')
      )
      .join('\n\n');
    if (notices.includes('*/')) {
      throw Error('License requires a safe block-comment representation');
    }
    const source =
      '/* Generated by tooling/runtime-js/build.mjs. Do not edit.\n' +
      notices +
      '\n*/\n' +
      "(function (host) {\n'use strict';\nconst require=host.require;const module={exports:{}};const exports=module.exports;\n" +
      result.outputFiles[0].text +
      '\nconst factory=module.exports.default;return factory(host);\n})\n';
    save(`${name}.js`, source, manifest);
    save(`${name}.LICENSE.txt`, notices.trimEnd() + '\n', manifest);
    manifest.bundles[name] = {
      imports,
      sources,
      packages: records.map(({ notices, ...record }) => ({
        ...record,
        notices: notices.map((item) => ({ path: item.path, sha256: digest(item.text) }))
      }))
    };
  }
  const text = JSON.stringify(manifest, null, 2) + '\n';
  const file = path.join(destination, 'manifest.json');
  if (check) {
    if (fs.readFileSync(file, 'utf8') !== text) {
      throw Error('Generated manifest is stale');
    }
  } else if (!fs.existsSync(file) || fs.readFileSync(file, 'utf8') !== text) {
    fs.writeFileSync(file, text);
  }
  process.stdout.write(
    check
      ? 'Runtime JS bundles reproduce exactly.\n'
      : 'Runtime JS bundles generated with pinned inputs and notices.\n'
  );
})().catch((error) => {
  process.stderr.write(String(error) + '\n');
  process.exitCode = 1;
});
