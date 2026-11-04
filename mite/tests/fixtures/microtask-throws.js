setTimeout(() => console.log('WRONG timer after exception'), 10);
queueMicrotask(() => { throw new Error('microtask fixture'); });
queueMicrotask(() => console.log('WRONG microtask after exception'));
