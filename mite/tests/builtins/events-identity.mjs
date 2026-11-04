import E, { EventEmitter, errorMonitor } from 'node:events';
import Alias from 'events';
import CommonJS from './events-export.cjs';
console.log(E === EventEmitter, E === Alias, E === CommonJS, E.errorMonitor === errorMonitor);
console.log(await import('events') === await import('node:events'));
const emitter = new E();
console.log(emitter instanceof CommonJS);
try { await import('node:events', { with: { type: 'json' } }); }
catch (error) { console.log(error.name, error.code); }
