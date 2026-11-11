/** Add audited native adapters to an existing stage without replacing its dependency tree. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';

const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');

export function verifyPackageAdditions(output, previous, plans) {
  const old = new Map();
  const selected = new Map();
  for (const record of previous) {
    if (old.has(record.name)) {
      throw Error('Duplicate previous package adapter');
    }
    old.set(record.name, record);
  }
  for (const { plan } of plans) {
    if (selected.has(plan.record.name)) {
      throw Error('Duplicate selected package adapter');
    }
    selected.set(plan.record.name, plan.record);
  }
  for (const [name, record] of old) {
    if (JSON.stringify(record) !== JSON.stringify(selected.get(name))) {
      throw Error('Refresh must preserve existing package adapter: ' + name);
    }
  }
  for (const [name, record] of selected) {
    if (
      record.additionalFiles !== undefined &&
      (!Array.isArray(record.additionalFiles) || record.additionalFiles.length > 16)
    ) {
      throw Error('Invalid additional package adapter files');
    }
    if (
      record.auditFiles !== undefined &&
      (!Array.isArray(record.auditFiles) || record.auditFiles.length > 512)
    ) {
      throw Error('Invalid audited package runtime files');
    }
    const paths = new Set();
    const audits = (record.auditFiles ?? []).map((file) => ({
      path: file.path,
      sourceSha256: file.sha256,
      outputSha256: file.sha256
    }));
    for (const file of [record, ...(record.additionalFiles ?? []), ...audits]) {
      if (!file || paths.has(file.path)) {
        throw Error('Duplicate package adapter file');
      }
      paths.add(file.path);
      if (
        typeof file.path !== 'string' ||
        path.isAbsolute(file.path) ||
        file.path.split(/[\\/]/).some((part) => !part || part === '.' || part === '..')
      ) {
        throw Error('Invalid package adapter target');
      }
      const target = fs.realpathSync(path.join(output, file.path));
      const modules = path.join(output, 'node_modules') + path.sep;
      if (!target.startsWith(modules) || !fs.statSync(target).isFile()) {
        throw Error('Package adapter target escaped stage');
      }
      const expected = old.has(name) ? file.outputSha256 : file.sourceSha256;
      if (hash(fs.readFileSync(target)) !== expected) {
        throw Error('Refuse edited native package loader: ' + name);
      }
      let directory = path.dirname(target);
      while (
        directory.startsWith(modules) &&
        !fs.existsSync(path.join(directory, 'package.json'))
      ) {
        directory = path.dirname(directory);
      }
      if (!directory.startsWith(modules)) {
        throw Error('Missing native package manifest');
      }
      const packageFile = fs.realpathSync(path.join(directory, 'package.json'));
      if (!packageFile.startsWith(modules)) {
        throw Error('Native package manifest escaped stage');
      }
      const bytes = fs.readFileSync(packageFile);
      const metadata = JSON.parse(bytes);
      if (
        hash(bytes) !== record.packageSha256 ||
        metadata.name !== name ||
        metadata.version !== record.version
      ) {
        throw Error('Refuse edited native package manifest: ' + name);
      }
    }
  }
  return plans.filter(({ plan }) => !old.has(plan.record.name));
}
