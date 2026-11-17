import { Writable } from 'node:stream';
import checkWritable from './cases.mjs';
await checkWritable(Writable);
