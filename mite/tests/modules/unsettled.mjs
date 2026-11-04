console.log('before');
await new Promise(() => {});
console.log('WRONG settled');
