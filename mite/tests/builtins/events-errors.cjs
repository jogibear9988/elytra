const E = require('events');
const emitter = new E();
const error = new Error('original');
emitter.on(E.errorMonitor, value => console.log('monitor', value === error));
emitter.on('error', value => console.log('handled', value === error));
console.log(emitter.emit('error', error));
const onlyMonitor = new E();
onlyMonitor.on(E.errorMonitor, () => console.log('monitored'));
try { onlyMonitor.emit('error', error); } catch (caught) { console.log(caught === error); }
try { new E().emit('error', 'context'); } catch (caught) { console.log(caught.code, caught.context); }
let second = false;
const throwing = new E();
throwing.on('x', () => { throw error; });
throwing.on('x', () => { second = true; });
try { throwing.emit('x'); } catch (caught) { console.log(caught === error, second); }
