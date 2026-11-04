const timer = setTimeout(() => console.log('WRONG unref'), 10000);
console.log(timer.hasRef());
console.log(timer.unref() === timer, timer.hasRef());
console.log(timer.ref() === timer, timer.hasRef());
timer.unref();
