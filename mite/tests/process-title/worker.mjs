import process from 'node:process';
import { parentPort } from 'node:worker_threads';
const before = process.title;
let coercions = 0;
const value = {
  local: 'worker-title',

  toString() {
    coercions++;
    return 'wrk';
  }
};
process.title = value;
const descriptor = Object.getOwnPropertyDescriptor(process, 'title');
parentPort.postMessage({
  before,
  after: { local: process.title.local },
  coercions,
  pid: process.pid,
  dataDescriptor:
    descriptor.value === value &&
    descriptor.writable &&
    descriptor.enumerable &&
    descriptor.configurable
});
parentPort.close();
