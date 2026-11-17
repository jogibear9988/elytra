/** Fail before bundling if the private protocol provider or its intact MIT notice has drifted. */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const root = path.dirname(fileURLToPath(import.meta.url));

export function verifyWebSocketSources(directory = path.join(root, 'node_modules/ws')) {
  const pin = JSON.parse(fs.readFileSync(path.join(root, 'ws-engine-pin.json'), 'utf8'));
  const lock = JSON.parse(fs.readFileSync(path.join(root, 'package-lock.json'), 'utf8'));
  const record = lock.packages['node_modules/ws'];
  if (
    pin.version !== '8.21.0' ||
    record?.version !== pin.version ||
    record.integrity !== pin.integrity
  ) {
    throw Error('Private WebSocket provider lock drifted');
  }
  for (const [name, expected] of Object.entries(pin.sha256)) {
    const digest = createHash('sha256')
      .update(fs.readFileSync(path.join(directory, name)))
      .digest('hex');
    if (digest !== expected) {
      throw Error('Private WebSocket provider source drifted: ' + name);
    }
  }
  return pin;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  verifyWebSocketSources(process.argv[2]);
  console.log('Pinned private ws sources and MIT notice verified.');
}
