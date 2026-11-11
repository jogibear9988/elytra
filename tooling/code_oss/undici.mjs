/** Audited undici7.29.1 metadata adaptation; never invent a Node runtime/version. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const repository = fileURLToPath(new URL('../../', import.meta.url));
const pin = JSON.parse(fs.readFileSync(path.join(repository, 'vendor/undici/manifest.json')));

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

const probe =
  "const [nodeMajor, nodeMinor] = process.versions.node.split('.', 2).map(v => Number(v))";
const replacement = `const __elytraMiteUndici = typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined
const [nodeMajor, nodeMinor] = __elytraMiteUndici ? [undefined, undefined] : process.versions.node.split('.', 2).map(v => Number(v))
if (__elytraMiteUndici) {
  if (Object.hasOwn(process, '_vscodeLibJSUndiciStatus')) {
    throw Object.assign(new Error('Conflicting undici Node metadata status'), {code:'ERR_CODE_OSS_UNDICI_METADATA'})
  }
  Object.defineProperty(process, '_vscodeLibJSUndiciStatus', {
    value: Object.freeze({package:'undici',version:'7.29.1',feature:'node-version-metadata',available:false,reason:'Mite embeds LibJS without a Node runtime; unused Node version exports remain undefined'}),
    enumerable:false, configurable:false, writable:false
  })
}`;

export function transformUndici(bytes) {
  if (digest(bytes) !== pin.sourceSha256 || bytes.toString('utf8').split(probe).length !== 2) {
    throw Error('Undici7.29.1 Node version probe drift');
  }
  return Buffer.from(bytes.toString('utf8').replace(probe, replacement));
}

const featurePath = 'lib/util/runtime-features.js';
const featureSha256 = pin.runtimeFiles.find((file) => file.path === featurePath).sha256;
const featureProbe = 'function detectRuntimeFeatureByExportedProperty (moduleName, property) {';

export function transformUndiciFeatures(bytes) {
  if (digest(bytes) !== featureSha256 || bytes.toString('utf8').split(featureProbe).length !== 2) {
    throw Error('Undici7.29.1 optional feature probe drift');
  }
  return Buffer.from(
    bytes.toString('utf8').replace(
      featureProbe,
      'let __elytraUndiciFeatureStatus\n\n' +
        featureProbe +
        `
  if (typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined && !require('node:module').isBuiltin(moduleName)) {
    if (moduleName !== 'node:worker_threads' || property !== 'markAsUncloneable') {
      throw Object.assign(new Error('Unaudited undici optional feature'), {code:'ERR_CODE_OSS_UNDICI_FEATURE'})
    }
    const key = '_vscodeLibJSUndiciFeaturesStatus'
    if (!__elytraUndiciFeatureStatus) {
      if (Object.hasOwn(process,key)) throw Object.assign(new Error('Conflicting undici feature status'), {code:'ERR_CODE_OSS_UNDICI_FEATURE'})
      const status = Object.freeze({package:'undici',version:'7.29.1',module:'node:worker_threads',feature:'markAsUncloneable',available:false,reason:'The native builtin registry reports no worker_threads implementation'})
      Object.defineProperty(process,key,{value:status,enumerable:false,writable:false,configurable:false})
      __elytraUndiciFeatureStatus = status
    }
    return false
  }`
    )
  );
}

const webidlPath = 'lib/web/webidl/index.js';
const webidlSha256 = pin.runtimeFiles.find((file) => file.path === webidlPath).sha256;
const webidlProbe = 'webidl.is.MessagePort = webidl.util.MakeTypeAssertion(MessagePort)';

export function transformUndiciWebIDL(bytes) {
  if (digest(bytes) !== webidlSha256 || bytes.toString('utf8').split(webidlProbe).length !== 2) {
    throw Error('Undici7.29.1 ambient MessagePort assertion drift');
  }
  return Buffer.from(
    bytes.toString('utf8').replace(
      webidlProbe,
      `const __elytraMiteNoMessagePort = typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined && typeof MessagePort === 'undefined'
webidl.is.MessagePort = __elytraMiteNoMessagePort ? () => false : webidl.util.MakeTypeAssertion(MessagePort)
if (__elytraMiteNoMessagePort) {
  const key = '_vscodeLibJSUndiciWebIDLStatus'
  if (Object.hasOwn(process,key)) throw Object.assign(new Error('Conflicting undici WebIDL status'),{code:'ERR_CODE_OSS_UNDICI_WEBIDL'})
  Object.defineProperty(process,key,{value:Object.freeze({package:'undici',version:'7.29.1',feature:'ambient-MessagePort',available:false,reason:'Mite has no ambient MessagePort constructor; the optional type assertion cannot match'}),enumerable:false,writable:false,configurable:false})
}`
    )
  );
}

/** Every runtime JS file is pinned: the two metadata exports have no consumers. */
export function planUndici(root) {
  root = fs.realpathSync(root);
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, 'undici'));
  if (!location.startsWith(modules + path.sep)) {
    throw Error('Undici escapes dependency tree');
  }
  const packageBytes = fs.readFileSync(path.join(location, 'package.json'));
  const metadata = JSON.parse(packageBytes);
  if (
    metadata.name !== pin.name ||
    metadata.version !== pin.version ||
    digest(packageBytes) !== pin.packageSha256
  ) {
    throw Error('Undici adaptation requires exact audited package7.29.1');
  }
  const runtime = [];

  const visit = (directory, prefix) => {
    for (const name of fs.readdirSync(directory)) {
      const filename = path.join(directory, name);
      const relative = prefix + name;
      const info = fs.lstatSync(filename);
      if (info.isDirectory() && (prefix || name === 'lib')) {
        visit(filename, relative + '/');
      } else if (info.isFile() && name.endsWith('.js')) {
        runtime.push(relative);
      } else if (info.isSymbolicLink()) {
        throw Error('Undici runtime symlink is unaudited');
      }
    }
  };

  visit(location, '');
  if (
    JSON.stringify(runtime.sort()) !== JSON.stringify(pin.runtimeFiles.map((file) => file.path))
  ) {
    throw Error('Undici runtime file inventory drift');
  }
  for (const file of pin.runtimeFiles) {
    if (digest(fs.readFileSync(path.join(location, file.path))) !== file.sha256) {
      throw Error('Undici runtime source drift: ' + file.path);
    }
  }
  const bytes = transformUndici(fs.readFileSync(path.join(location, 'lib/core/util.js')));
  const webidlBytes = transformUndiciWebIDL(fs.readFileSync(path.join(location, webidlPath)));
  const featureBytes = transformUndiciFeatures(fs.readFileSync(path.join(location, featurePath)));

  const relative = (filename) =>
    path.relative(root, path.join(location, filename)).split(path.sep).join('/');

  return {
    bytes,
    featureBytes,
    webidlBytes,
    record: {
      name: pin.name,
      version: pin.version,
      path: relative('lib/core/util.js'),
      sourceSha256: pin.sourceSha256,
      outputSha256: digest(bytes),
      packageSha256: pin.packageSha256,
      adaptation:
        'undefined-node-metadata-native-optional-feature-query-and-ambient-messageport-trait',
      additionalFiles: [
        {
          path: relative(featurePath),
          sourceSha256: featureSha256,
          outputSha256: digest(featureBytes)
        },
        {
          path: relative(webidlPath),
          sourceSha256: webidlSha256,
          outputSha256: digest(webidlBytes)
        }
      ],
      condition: 'actual Mite without Node/Electron version markers',
      auditFiles: pin.runtimeFiles
        .filter(
          (file) =>
            file.path !== 'lib/core/util.js' &&
            file.path !== featurePath &&
            file.path !== webidlPath
        )
        .map((file) => ({ path: relative(file.path), sha256: file.sha256 })),
      adapterSource: {
        path: 'tooling/code_oss/undici.mjs',
        sha256: digest(fs.readFileSync(fileURLToPath(import.meta.url)))
      }
    }
  };
}

export function applyUndici(root, plan) {
  root = fs.realpathSync(root);
  const changes = [
    { record: plan.record, bytes: plan.bytes },
    { record: plan.record.additionalFiles[0], bytes: plan.featureBytes },
    { record: plan.record.additionalFiles[1], bytes: plan.webidlBytes }
  ];
  const prepared = changes.map(({ record, bytes }) => {
    const filename = fs.realpathSync(path.join(root, record.path));
    if (
      !filename.startsWith(root + path.sep) ||
      digest(fs.readFileSync(filename)) !== record.sourceSha256
    ) {
      throw Error('Copied undici target escaped or drifted');
    }
    return { filename, bytes };
  });
  for (const file of plan.record.auditFiles) {
    const target = fs.realpathSync(path.join(root, file.path));
    if (!target.startsWith(root + path.sep) || digest(fs.readFileSync(target)) !== file.sha256) {
      throw Error('Copied undici runtime drift');
    }
  }
  for (const { filename, bytes } of prepared) {
    fs.writeFileSync(filename, bytes);
  }
}
