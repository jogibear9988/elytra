console.log('Hello from Mite and LibJS!');
console.log(JSON.stringify(process.argv.slice(2)));
Promise.resolve().then(() => console.log('Promise jobs work.'));
