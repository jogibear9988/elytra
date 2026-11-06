import process from 'node:process';

console.log('Hello from Mite and LibJS!');
console.log(JSON.stringify(process.argv.slice(2)));
await Promise.resolve();
console.log('Promise jobs work.');
