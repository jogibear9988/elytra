/** An unreferenced immediate cannot keep an otherwise idle process alive. */
setImmediate(() => console.log('UNEXPECTED')).unref();
console.log('exit');
