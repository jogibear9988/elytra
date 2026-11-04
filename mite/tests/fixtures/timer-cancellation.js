clearTimeout(setTimeout(() => console.log('WRONG object'), 1));
clearTimeout(Number(setTimeout(() => console.log('WRONG numeric'), 1)));
clearTimeout(String(Number(setTimeout(() => console.log('WRONG string'), 1))));
clearInterval(setInterval(() => console.log('WRONG interval'), 1));
const timer = setTimeout(() => console.log('kept'), 1);
clearTimeout(Number(timer) + 0.5);
clearTimeout('0' + Number(timer));
clearTimeout(null);
clearTimeout({ valueOf() { throw new Error('must not coerce'); } });
let count = 0;
const interval = setInterval(function (increment) {
    count += increment;
    console.log(count, this === interval);
    if (count === 3) clearTimeout(interval);
}, 1, 1);
