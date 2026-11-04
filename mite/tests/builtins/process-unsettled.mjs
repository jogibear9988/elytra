process.exitCode = 5;
console.log('pending');
await new Promise(() => {});
