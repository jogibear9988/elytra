/** Compose the Node 24 synchronous Writable admission rule without editing provider files. */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.dirname(directory);

const digest = (source) => createHash('sha256').update(source).digest('hex');

const pin = JSON.parse(fs.readFileSync(path.join(directory, 'writable-source-pin.json'), 'utf8'));

const before = `  // stream._write resets state.length
  const ret = state.length < state.highWaterMark
  // We must ensure that previous needDrain will not be reset to false.
  if (!ret) state.needDrain = true
`;
const after = `  // Node 24 decides admission after a synchronous _write has released bytes.
  const ret = state.length < state.highWaterMark || state.length === 0
  // We must ensure that previous needDrain will not be reset to false.
  if (!ret) state.needDrain = true
`;
const insertion = `  // Return false if errored or destroyed in order to break
`;

export function composeWritableSource(source) {
  if (digest(source) !== pin.sha256[pin.entry]) {
    throw Error('Writable admission provider source drifted');
  }
  if (source.split(before).length !== 2 || source.split(insertion).length !== 2) {
    throw Error('Writable admission cleanup seam changed');
  }
  const composed = source.replace(before, '').replace(insertion, after + '\n' + insertion);
  if (
    composed.split(after).length !== 2 ||
    composed
      .replace(after + '\n', '')
      .replace('  if (state.writing ||', before + '  if (state.writing ||') !== source ||
    digest(composed) !== pin.composedSha256
  ) {
    throw Error('Writable admission composition/reversal changed');
  }
  return composed;
}

export function verifyWritableSources(provider = path.join(root, 'node_modules/readable-stream')) {
  const lock = JSON.parse(fs.readFileSync(path.join(root, 'package-lock.json'), 'utf8'));
  const record = lock.packages['node_modules/readable-stream'];
  if (
    pin.name !== 'readable-stream' ||
    pin.version !== '4.7.0' ||
    record?.version !== pin.version ||
    record.integrity !== pin.integrity
  ) {
    throw Error('Writable admission provider lock drifted');
  }
  for (const [name, expected] of Object.entries(pin.sha256)) {
    if (digest(fs.readFileSync(path.join(provider, name))) !== expected) {
      throw Error('Writable admission provider source/license drifted: ' + name);
    }
  }
  composeWritableSource(fs.readFileSync(path.join(provider, pin.entry), 'utf8'));
  return pin;
}
