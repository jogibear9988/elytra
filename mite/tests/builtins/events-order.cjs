const E = require('events');
const emitter = new E();
const output = [];
function late() { output.push('late'); }
function second() { output.push('second'); }
emitter.on('change', () => {
    output.push('first');
    emitter.off('change', second);
    emitter.on('change', late);
});
emitter.on('change', second);
emitter.emit('change');
emitter.emit('change');
emitter.on('ordered', () => output.push('normal'));
emitter.prependListener('ordered', () => output.push('prepend'));
emitter.prependOnceListener('ordered', () => output.push('prependOnce'));
emitter.emit('ordered');
emitter.emit('ordered');
emitter.once('recursive', () => { output.push('once'); emitter.emit('recursive'); });
emitter.emit('recursive');
console.log(output.join(','));
