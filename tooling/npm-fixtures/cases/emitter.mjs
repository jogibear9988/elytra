import E, { EventEmitter } from 'eventemitter3';
import commonjs from './emitter-export.cjs';
import metadata from 'eventemitter3/package.json' with { type: 'json' };
console.log(E === EventEmitter, E === commonjs, metadata.name, metadata.version);
const emitter = new E();
const complete = new Promise((resolve) => emitter.once('complete', resolve));

setTimeout(() => emitter.emit('complete', 'native timer'), 1);

console.log(await complete);
