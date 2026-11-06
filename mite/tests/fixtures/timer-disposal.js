const closed = setTimeout(() => console.log('WRONG closed'), 1);
console.log(closed.close() === closed, closed.hasRef());
const disposed = setInterval(() => console.log('WRONG disposed'), 1);
console.log(typeof disposed[Symbol.dispose]);
disposed[Symbol.dispose]();
console.log(clearTimeout.name, clearInterval.name, clearTimeout === clearInterval);

setTimeout(() => console.log('done'), 1);
