/** Reuse only a verified owned compilation; never overwrite edited diagnostic output. */
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';

const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

function file(root, relative) {
  if (
    typeof relative !== 'string' ||
    path.isAbsolute(relative) ||
    relative.split(/[\\/]/).some((part) => !part || part === '.' || part === '..')
  ) {
    throw Error('Invalid owned compile output path');
  }
  const target = path.join(root, relative);
  const resolved = fs.realpathSync(target);
  if (resolved !== target || !fs.statSync(target).isFile()) {
    throw Error(`Owned compile output escaped or changed type: ${relative}`);
  }
  return target;
}

export function verifiedRefresh(output) {
  if (
    !fs.lstatSync(output).isDirectory() ||
    fs.lstatSync(output).isSymbolicLink() ||
    fs.realpathSync(output) !== output
  ) {
    throw Error('Refresh requires a physical canonical compiled directory');
  }
  const bytes = fs.readFileSync(file(output, 'ELYTRA-COMPILE.json'));
  const previous = JSON.parse(bytes);
  if (
    previous.schema !== 1 ||
    previous.kind !== 'diagnostic-source-tree' ||
    previous.compiler !== 'esbuild 0.25.12' ||
    !Array.isArray(previous.inputs)
  ) {
    throw Error('Refresh requires the owned pinned compiler manifest');
  }
  const owned = new Set();
  for (const record of [
    ...previous.inputs,
    ...(previous.generated ?? []),
    ...(previous.rendererAssets?.generated ?? []),
    ...(previous.rootFiles ?? [])
  ]) {
    if (owned.has(record.output)) {
      throw Error(`Duplicate owned output: ${record.output}`);
    }
    owned.add(record.output);
    if (digest(fs.readFileSync(file(output, record.output))) !== record.outputSha256) {
      throw Error(`Refuse edited/stale compiled output: ${record.output}`);
    }
  }
  for (const record of previous.packageAdaptations ?? []) {
    // A prepared package may contain internal symlinks; its final target must
    // remain within this stage and equal the previously accepted adaptation.
    const target = fs.realpathSync(path.join(output, record.path));
    if (
      !target.startsWith(output + path.sep) ||
      digest(fs.readFileSync(target)) !== record.outputSha256
    ) {
      throw Error(`Edited or escaped package adaptation: ${record.path}`);
    }
  }
  for (const alias of previous.rendererAssets?.aliases ?? []) {
    const target = path.join(output, alias.path);
    if (
      alias.path !== 'out' ||
      alias.target !== 'src' ||
      !fs.lstatSync(target).isSymbolicLink() ||
      fs.readlinkSync(target) !== 'src'
    ) {
      throw Error('Compiled renderer alias drift');
    }
  }
  return { previous, owned, manifestSha256: digest(bytes) };
}

export function checkRefreshPlan(output, refresh, planned) {
  for (const name of refresh.owned) {
    if (!planned.has(name)) {
      throw Error(`Refresh would orphan owned output: ${name}`);
    }
  }
  for (const [name, bytes] of planned) {
    if (name === 'ELYTRA-COMPILE.json') {
      continue;
    }
    if (
      path.isAbsolute(name) ||
      name.split(/[\\/]/).some((part) => !part || part === '.' || part === '..')
    ) {
      throw Error('Invalid planned compile output path');
    }
    const target = path.join(output, name);
    // New outputs must not follow an unrecorded parent symlink outside the stage.
    for (
      let directory = path.dirname(target);
      directory !== output;
      directory = path.dirname(directory)
    ) {
      const status = fs.lstatSync(directory, { throwIfNoEntry: false });
      if (status && (!status.isDirectory() || status.isSymbolicLink())) {
        throw Error(`Unowned output parent changed type: ${name}`);
      }
    }
    try {
      fs.lstatSync(target);
    } catch (error) {
      if (error.code === 'ENOENT') {
        continue;
      }
      throw error;
    }
    // New files may exist only if already byte-identical. Never claim an edited
    // unrecorded file by overwriting it, including root metadata from old builds.
    const current = fs.readFileSync(file(output, name));
    if (!refresh.owned.has(name) && !current.equals(bytes)) {
      throw Error(`Unowned output collision during refresh: ${name}`);
    }
  }
}
