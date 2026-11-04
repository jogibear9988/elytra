console.log('start');
Promise.resolve().then(() => {
    console.log('first');
    Promise.resolve().then(() => console.log('nested'));
});
Promise.resolve().then(() => console.log('second'));
console.log('end');
