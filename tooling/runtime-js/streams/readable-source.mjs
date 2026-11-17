/** Compose one current Node iterator cleanup rule without editing provider files. */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.dirname(directory);

const digest = (source) => createHash('sha256').update(source).digest('hex');

const pin = JSON.parse(fs.readFileSync(path.join(directory, 'readable-source-pin.json'), 'utf8'));

const before = `  } finally {
    if (
      (error || (options === null || options === undefined ? undefined : options.destroyOnReturn) !== false) &&
      (error === undefined || stream._readableState.autoDestroy)
    ) {
`;
const after = `  } finally {
    const preserveHalfOpenDuplex =
      error === null &&
      stream.allowHalfOpen === true &&
      stream.writable === true &&
      stream.writableEnded !== true
    if (
      (error || (options === null || options === undefined ? undefined : options.destroyOnReturn) !== false) &&
      (error === undefined || stream._readableState.autoDestroy) &&
      !preserveHalfOpenDuplex
    ) {
`;

export function composeReadableSource(source) {
  if (digest(source) !== pin.sha256[pin.entry]) {
    throw Error('Readable iterator provider source drifted');
  }
  if (source.split(before).length !== 2 || source.includes('preserveHalfOpenDuplex')) {
    throw Error('Readable iterator cleanup seam changed');
  }
  const composed = source.replace(before, after);
  if (
    composed.split(after).length !== 2 ||
    composed.replace(after, before) !== source ||
    digest(composed) !== pin.composedSha256
  ) {
    throw Error('Readable iterator composition/reversal changed');
  }
  return composed;
}

export function verifyReadableSources(provider = path.join(root, 'node_modules/readable-stream')) {
  const lock = JSON.parse(fs.readFileSync(path.join(root, 'package-lock.json'), 'utf8'));
  const record = lock.packages['node_modules/readable-stream'];
  if (
    pin.name !== 'readable-stream' ||
    pin.version !== '4.7.0' ||
    record?.version !== pin.version ||
    record.integrity !== pin.integrity
  ) {
    throw Error('Readable iterator provider lock drifted');
  }
  for (const [name, expected] of Object.entries(pin.sha256)) {
    if (digest(fs.readFileSync(path.join(provider, name))) !== expected) {
      throw Error('Readable iterator provider source/license drifted: ' + name);
    }
  }
  composeReadableSource(fs.readFileSync(path.join(provider, pin.entry), 'utf8'));
  return pin;
}
