/** Exact async UUID seam in deviceid0.1.5; no substitute identity or storage backend. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const sourceSha256 = '0d30d86857783615a35a42e14f3770d4fc956769c9dd0911e054ad390805df72';
const packageSha256 = '031958ead47834a8069d5da665f729d0965e87e1f91eca7044fd75b617dfafc3';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function transformDeviceId(bytes) {
  if (digest(bytes) !== sourceSha256) {
    throw Error('Deviceid0.1.5 source drift');
  }
  const source = bytes.toString('utf8');
  const load = 'const uuid_1 = require("uuid");';
  const generate = 'const newDeviceId = (0, uuid_1.v4)().toLowerCase();';
  if (source.split(load).length !== 2 || source.split(generate).length !== 2) {
    throw Error('Unexpected deviceid UUID seam');
  }
  return Buffer.from(
    source
      .replace(
        load,
        `const uuid_1 = typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined ? undefined : require("uuid");`
      )
      .replace(
        generate,
        'const newDeviceId = (0, (uuid_1 ?? await import("uuid")).v4)().toLowerCase();'
      )
  );
}

export function transformDeviceIdStorage(bytes) {
  const expected = '518013180a98b70fa922cc3a6a9ef00a9b27f4eed3e39cf37cee93ab44f42e38';
  if (digest(bytes) !== expected) {
    throw Error('Deviceid0.1.5 storage drift');
  }
  const source = bytes.toString('utf8');
  const load = 'const fs = __importStar(require("fs-extra"));';
  const access = 'await fs.promises.access(path);';
  const directory = 'await fs.ensureDir(getDirectory());';
  for (const seam of [load, access, directory]) {
    if (source.split(seam).length !== 2) {
      throw Error('Unexpected deviceid storage seam');
    }
  }
  return Buffer.from(
    source
      .replace(
        load,
        `const __elytraMiteStorage = typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined;
const fs = __elytraMiteStorage ? require("node:fs/promises") : __importStar(require("fs-extra"));`
      )
      .replace(access, 'await (__elytraMiteStorage ? fs.access(path) : fs.promises.access(path));')
      .replace(
        directory,
        'if (__elytraMiteStorage) { await fs.mkdir(getDirectory(), { recursive: true }); } else { await fs.ensureDir(getDirectory()); }'
      )
  );
}

export function planDeviceId(root) {
  const modules = fs.realpathSync(path.join(root, 'node_modules'));
  const location = fs.realpathSync(path.join(modules, '@vscode/deviceid'));
  const relative = path.relative(modules, location);
  if (relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
    throw Error('Deviceid package escapes prepared dependency tree');
  }
  const packageBytes = fs.readFileSync(path.join(location, 'package.json'));
  const manifest = JSON.parse(packageBytes);
  if (
    manifest.name !== '@vscode/deviceid' ||
    manifest.version !== '0.1.5' ||
    manifest.main !== './dist/index.js' ||
    digest(packageBytes) !== packageSha256
  ) {
    throw Error('Deviceid adaptation requires exact audited package0.1.5');
  }
  const source = fs.readFileSync(path.join(location, 'dist/devdeviceid.js'));
  const bytes = transformDeviceId(source);
  const storage = fs.readFileSync(path.join(location, 'dist/storage.js'));
  const storageBytes = transformDeviceIdStorage(storage);
  const filename = fileURLToPath(import.meta.url);
  const repository = fileURLToPath(new URL('../../', import.meta.url));
  const record = {
    name: manifest.name,
    version: manifest.version,
    path: path.relative(root, path.join(location, 'dist/devdeviceid.js')).split(path.sep).join('/'),
    sourceSha256,
    outputSha256: digest(bytes),
    packageSha256,
    adaptation: 'async-genuine-uuid-import-and-native-fs-promises',
    additionalFiles: [
      {
        path: path.relative(root, path.join(location, 'dist/storage.js')).split(path.sep).join('/'),
        sourceSha256: digest(storage),
        outputSha256: digest(storageBytes)
      }
    ],
    condition: 'actual Mite without Node/Electron version markers',
    adapterSource: {
      path: path.relative(repository, filename).split(path.sep).join('/'),
      sha256: digest(fs.readFileSync(filename))
    }
  };
  return { record, bytes, storageBytes };
}

export function applyDeviceId(root, plan) {
  const changes = [
    { record: plan.record, bytes: plan.bytes },
    { record: plan.record.additionalFiles[0], bytes: plan.storageBytes }
  ];
  const prepared = changes.map(({ record, bytes }) => {
    const resolved = fs.realpathSync(path.join(root, record.path));
    const relative = path.relative(fs.realpathSync(root), resolved);
    if (
      relative === '..' ||
      relative.startsWith('..' + path.sep) ||
      path.isAbsolute(relative) ||
      digest(fs.readFileSync(resolved)) !== record.sourceSha256
    ) {
      throw Error('Copied deviceid target escaped or changed before adaptation');
    }
    return { resolved, bytes };
  });
  for (const { resolved, bytes } of prepared) {
    fs.writeFileSync(resolved, bytes);
  }
}
