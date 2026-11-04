console.log('before');
await new Promise(resolve => setTimeout(resolve, 1));
console.log('after');
